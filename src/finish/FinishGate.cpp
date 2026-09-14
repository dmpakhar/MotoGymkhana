#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VL53L1X.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config/Config.h"
#include "gate/GateDetector.h"
#include "protocol/DisplayStatusProtocol.h"
#include "protocol/FinishGateProtocol.h"

namespace
{
constexpr uint8_t kOledSdaPin = 10;
constexpr uint8_t kOledSclPin = 11;
constexpr uint8_t kOledAddress = 0x3C;
constexpr uint8_t kBatteryAdcPin = 1;
constexpr uint8_t kTofSdaPin = 6;
constexpr uint8_t kTofSclPin = 7;
constexpr uint8_t kTofAddress = 0x29;
constexpr uint16_t kTofTimingBudgetMs = 50;
constexpr float kBatteryDividerRatio = 2.0F; // Two equal 100k resistors.
constexpr uint16_t kBatteryMinimumMv = 3400;
constexpr uint16_t kBatteryMaximumMv = 4200;
constexpr uint8_t kBatterySegments = 4;
constexpr uint8_t kAdcSamples = 16;
constexpr uint32_t kDisplayRefreshIntervalMs = 100;
constexpr uint32_t kBatteryReadIntervalMs = 2000;
constexpr uint32_t kCrossingNoticeMs = 600;
constexpr uint8_t kBroadcastAddress[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

class FinishTofSensor final : public Adafruit_VL53L1X
{
public:
    FinishTofSensor()
        : Adafruit_VL53L1X()
    {
        gpio0 = -1;
    }
};

FinishTofSensor tof;
bool tofReady = false;
int16_t distanceMm = -1;
GateDetector gateDetector;
bool espNowReady = false;
uint32_t bootId = 0;
uint32_t frameSequence = 0;
uint32_t crossingCount = 0;
uint32_t lastHeartbeatMs = 0;
volatile uint32_t lastMainPacketMs = 0;
uint32_t lastCrossingMs = 0;
volatile bool commandPending = false;
FinishGateProtocol::Command pendingCommand;
portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
bool sensorSetupActive = false;
uint32_t appliedCommandId = 0;

bool respondsAt(TwoWire &bus, uint8_t address)
{
    bus.beginTransmission(address);
    return bus.endTransmission() == 0;
}

void receiveEspNow(const uint8_t *, const uint8_t *data, int length)
{
    if (length == static_cast<int>(sizeof(FinishGateProtocol::Command)))
    {
        FinishGateProtocol::Command command;
        memcpy(&command, data, sizeof(command));
        if (command.magic == FinishGateProtocol::kCommandMagic &&
            command.version == FinishGateProtocol::kVersion)
        {
            portENTER_CRITICAL(&commandMux);
            pendingCommand = command;
            commandPending = true;
            portEXIT_CRITICAL(&commandMux);
        }
        return;
    }
    if (length != static_cast<int>(sizeof(DisplayStatusProtocol::Frame)))
    {
        return;
    }
    DisplayStatusProtocol::Frame frame;
    memcpy(&frame, data, sizeof(frame));
    if (frame.magic == DisplayStatusProtocol::kMagic &&
        frame.version == DisplayStatusProtocol::kVersion)
    {
        lastMainPacketMs = millis();
    }
}

void applyPendingCommand()
{
    FinishGateProtocol::Command command;
    portENTER_CRITICAL(&commandMux);
    const bool pending = commandPending;
    if (pending)
    {
        command = pendingCommand;
        commandPending = false;
    }
    portEXIT_CRITICAL(&commandMux);
    if (!pending || command.commandId == appliedCommandId)
    {
        return;
    }

    switch (command.type)
    {
    case FinishGateProtocol::CommandType::SetSetupActive:
        sensorSetupActive = command.value != 0;
        break;
    case FinishGateProtocol::CommandType::SetTriggerDistance:
        if (sensorSetupActive)
        {
            gateDetector.setTriggerDistanceMm(command.value);
        }
        break;
    case FinishGateProtocol::CommandType::UseCurrentDistance:
        if (sensorSetupActive && distanceMm > 0)
        {
            gateDetector.setTriggerDistanceMm(distanceMm);
        }
        break;
    }
    appliedCommandId = command.commandId;
    Serial.print("Applied finish command #");
    Serial.println(appliedCommandId);
}

void beginEspNow()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if (esp_wifi_set_channel(Config::kWifiAccessPointChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
        esp_now_init() != ESP_OK)
    {
        Serial.println("ESP-NOW initialization failed");
        return;
    }

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, kBroadcastAddress, ESP_NOW_ETH_ALEN);
    peer.channel = Config::kWifiAccessPointChannel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK || esp_now_register_recv_cb(receiveEspNow) != ESP_OK)
    {
        Serial.println("ESP-NOW peer/receiver setup failed");
        return;
    }
    espNowReady = true;
    Serial.println("ESP-NOW finish link ready");
}

void sendHeartbeat(uint16_t batteryMv)
{
    if (!espNowReady)
    {
        return;
    }
    FinishGateProtocol::Frame frame;
    frame.flags = tofReady ? FinishGateProtocol::SensorReady : 0;
    if (gateDetector.isCalibrated())
    {
        frame.flags |= FinishGateProtocol::Calibrated;
    }
    if (sensorSetupActive)
    {
        frame.flags |= FinishGateProtocol::SetupActive;
    }
    frame.batteryMv = batteryMv;
    frame.bootId = bootId;
    frame.sequence = ++frameSequence;
    frame.crossingCount = crossingCount;
    frame.appliedCommandId = appliedCommandId;
    frame.distanceMm = distanceMm;
    frame.filteredDistanceMm = gateDetector.filteredDistanceMm();
    frame.triggerDistanceMm = gateDetector.triggerDistanceMm();
    esp_now_send(kBroadcastAddress, reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
}

uint16_t readBatteryMillivolts()
{
    uint32_t sumMv = 0;
    for (uint8_t sample = 0; sample < kAdcSamples; ++sample)
    {
        sumMv += analogReadMilliVolts(kBatteryAdcPin);
        delay(2);
    }
    return static_cast<uint16_t>((sumMv / kAdcSamples) * kBatteryDividerRatio);
}

uint8_t batterySegments(uint16_t batteryMv)
{
    if (batteryMv <= kBatteryMinimumMv)
    {
        return 0;
    }
    if (batteryMv >= kBatteryMaximumMv)
    {
        return kBatterySegments;
    }

    const uint32_t rangeMv = kBatteryMaximumMv - kBatteryMinimumMv;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(batteryMv - kBatteryMinimumMv) * kBatterySegments) / rangeMv);
}

void drawBattery(uint16_t batteryMv)
{
    constexpr int16_t x = 93;
    constexpr int16_t y = 1;
    constexpr int16_t width = 31;
    constexpr int16_t height = 13;
    constexpr int16_t terminalWidth = 3;
    constexpr int16_t segmentWidth = 5;
    constexpr int16_t segmentGap = 1;

    oled.drawRect(x, y, width, height, SSD1306_WHITE);
    oled.fillRect(x + width, y + 4, terminalWidth, 5, SSD1306_WHITE);

    const uint8_t filled = batterySegments(batteryMv);
    for (uint8_t segment = 0; segment < kBatterySegments; ++segment)
    {
        const int16_t segmentX = x + 3 + segment * (segmentWidth + segmentGap);
        oled.drawRect(segmentX, y + 3, segmentWidth, 7, SSD1306_WHITE);
        if (segment < filled)
        {
            oled.fillRect(segmentX + 1, y + 4, segmentWidth - 2, 5, SSD1306_WHITE);
        }
    }
}

void render(uint16_t batteryMv, uint32_t nowMs)
{
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 3);
    oled.print("GYMIO");
    oled.setCursor(55, 3);
    oled.print(batteryMv / 1000.0F, 2);
    oled.print("V");
    drawBattery(batteryMv);

    oled.drawLine(0, 17, 127, 17, SSD1306_WHITE);
    oled.setCursor(3, 22);
    if (batteryMv <= kBatteryMinimumMv)
    {
        oled.setTextSize(1);
        oled.println("CHARGE BATTERY");
    }
    else if (!tofReady)
    {
        oled.setTextSize(1);
        oled.println("VL53L1X ERROR");
    }
    else if (!gateDetector.isCalibrated())
    {
        oled.setTextSize(1);
        oled.print("CALIBRATING ");
        oled.print(gateDetector.calibrationSampleCount());
        oled.print("/");
        oled.println(Config::kGateCalibrationSamples);
    }
    else if (sensorSetupActive)
    {
        oled.setTextSize(1);
        oled.println("SENSOR SETUP");
    }
    else if (lastCrossingMs != 0 && nowMs - lastCrossingMs <= kCrossingNoticeMs)
    {
        oled.setTextSize(1);
        oled.println("CROSSING DETECTED");
    }
    else
    {
        oled.setTextSize(1);
        const bool mainConnected = lastMainPacketMs != 0 &&
                                   millis() - lastMainPacketMs <= Config::kFinishGateTimeoutMs;
        oled.println(mainConnected ? "2 GATES CONNECTED" : "WAITING FOR START");
    }

    oled.setTextSize(2);
    oled.setCursor(3, 42);
    if (tofReady && distanceMm >= 0)
    {
        oled.print(distanceMm);
        oled.print("mm");
    }
    else if (tofReady)
    {
        oled.print("INF");
    }
    else
    {
        oled.print("FINISH");
    }
    if (gateDetector.isCalibrated())
    {
        oled.setTextSize(1);
        oled.setCursor(82, 55);
        oled.print("T");
        oled.print(gateDetector.triggerDistanceMm());
    }
    oled.display();
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("Gymio finish gate - ESP32-S3 Mini N4R2");

    analogReadResolution(12);
    analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
    Wire.begin(kOledSdaPin, kOledSclPin);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, kOledAddress))
    {
        Serial.println("SSD1306 initialization failed at 0x3C");
        while (true)
        {
            delay(1000);
        }
    }

    Wire1.begin(kTofSdaPin, kTofSclPin);
    bool tofFound = respondsAt(Wire1, kTofAddress);
    Serial.println(tofFound ? "VL53L1X found on SDA GPIO6 / SCL GPIO7"
                            : "No I2C response on SDA GPIO6 / SCL GPIO7");
    if (!tofFound)
    {
        Wire1.end();
        Wire1.begin(kTofSclPin, kTofSdaPin);
        tofFound = respondsAt(Wire1, kTofAddress);
        Serial.println(tofFound ? "VL53L1X found on SDA GPIO7 / SCL GPIO6"
                                : "No I2C response on SDA GPIO7 / SCL GPIO6");
    }

    tofReady = tofFound && tof.begin(kTofAddress, &Wire1);
    if (tofReady)
    {
        tof.startRanging();
        tof.setTimingBudget(kTofTimingBudgetMs);
        Serial.println("VL53L1X OK on SDA GPIO6 / SCL GPIO7");
    }
    else
    {
        Serial.println("VL53L1X initialization failed at 0x29");
    }

    bootId = esp_random();
    beginEspNow();
}

void loop()
{
    static uint32_t lastDisplayRefreshMs = 0;
    static uint32_t lastBatteryReadMs = 0;
    static uint16_t batteryMv = 0;
    const uint32_t nowMs = millis();
    applyPendingCommand();

    if (tofReady && tof.dataReady())
    {
        distanceMm = tof.distance();
        tof.clearInterrupt();
        const GateEvent gateEvent = gateDetector.update(distanceMm, nowMs);
        if (!sensorSetupActive && gateEvent == GateEvent::CrossingConfirmed)
        {
            ++crossingCount;
            lastCrossingMs = nowMs;
            Serial.print("Finish crossing #");
            Serial.println(crossingCount);
        }
        Serial.print("Distance: ");
        if (distanceMm < 0)
        {
            Serial.println("INF");
        }
        else
        {
            Serial.print(distanceMm);
            Serial.println(" mm");
        }
    }

    if (batteryMv == 0 || nowMs - lastBatteryReadMs >= kBatteryReadIntervalMs)
    {
        lastBatteryReadMs = nowMs;
        batteryMv = readBatteryMillivolts();
        Serial.print("Battery: ");
        Serial.print(batteryMv);
        Serial.print(" mV, segments: ");
        Serial.println(batterySegments(batteryMv));
    }

    if (nowMs - lastDisplayRefreshMs >= kDisplayRefreshIntervalMs)
    {
        lastDisplayRefreshMs = nowMs;
        render(batteryMv, nowMs);
    }

    if (nowMs - lastHeartbeatMs >= Config::kFinishGateHeartbeatMs)
    {
        lastHeartbeatMs = nowMs;
        sendHeartbeat(batteryMv);
    }
    delay(2);
}

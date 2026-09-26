#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config/Config.h"
#include "gate/GateDetector.h"
#include "protocol/DisplayStatusProtocol.h"
#include "protocol/FinishGateProtocol.h"
#include "tof/ToFSensor.h"

namespace
{
constexpr uint8_t kOledSdaPin = 10;
constexpr uint8_t kOledSclPin = 11;
constexpr uint8_t kOledAddress = 0x3C;
constexpr uint32_t kDisplayRefreshIntervalMs = 100;
constexpr uint32_t kCrossingNoticeMs = 600;
constexpr uint8_t kBroadcastAddress[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

ToFSensor tof;
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
        if (sensorSetupActive)
        {
            gateDetector.setTriggerFromReferenceDistanceMm(
                gateDetector.filteredDistanceMm());
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

void sendHeartbeat()
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
    frame.bootId = bootId;
    frame.sequence = ++frameSequence;
    frame.crossingCount = crossingCount;
    frame.appliedCommandId = appliedCommandId;
    frame.distanceMm = distanceMm;
    frame.filteredDistanceMm = gateDetector.filteredDistanceMm();
    frame.triggerDistanceMm = gateDetector.triggerDistanceMm();
    esp_now_send(kBroadcastAddress, reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
}

void render(uint32_t nowMs)
{
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 3);
    oled.print("GYMIO");
    oled.setCursor(84, 3);
    oled.print("FINISH");

    oled.drawLine(0, 17, 127, 17, SSD1306_WHITE);
    oled.setCursor(3, 22);
    if (!tofReady)
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
    delay(1000); // Match the main gate and allow the beam to clear before calibration.
    Serial.println("Gymio finish gate - ESP32-S3 Mini N4R2");

    Wire.begin(kOledSdaPin, kOledSclPin);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, kOledAddress))
    {
        Serial.println("SSD1306 initialization failed at 0x3C");
        while (true)
        {
            delay(1000);
        }
    }

    tofReady = tof.begin();
    if (tofReady)
    {
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
    const uint32_t nowMs = millis();
    applyPendingCommand();

    int16_t measuredDistanceMm = -1;
    if (tofReady && tof.readDistance(measuredDistanceMm))
    {
        distanceMm = measuredDistanceMm;
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

    if (nowMs - lastDisplayRefreshMs >= kDisplayRefreshIntervalMs)
    {
        lastDisplayRefreshMs = nowMs;
        render(nowMs);
    }

    if (nowMs - lastHeartbeatMs >= Config::kFinishGateHeartbeatMs)
    {
        lastHeartbeatMs = nowMs;
        sendHeartbeat();
    }
    delay(2);
}

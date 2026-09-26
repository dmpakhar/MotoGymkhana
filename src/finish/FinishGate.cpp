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
#include "protocol/GateControlProtocol.h"
#include "tof/ToFSensor.h"

namespace
{
constexpr uint8_t kOledSdaPin = 10;
constexpr uint8_t kOledSclPin = 11;
constexpr uint8_t kOledAddress = 0x3C;
constexpr uint32_t kDisplayRefreshIntervalMs = 100;
constexpr uint32_t kCrossingNoticeMs = 600;
constexpr uint32_t kGateConfiguredMessageMs = 3000;
constexpr uint8_t kGateButtonPin = 8;
constexpr uint8_t kBlueLedPin = 9;
constexpr uint8_t kYellowLedPin = 12;
constexpr uint8_t kGreenLedPin = 13;
constexpr uint32_t kButtonDebounceMs = 40;
constexpr uint32_t kButtonLongPressMs = 1500;
constexpr uint32_t kCommandRepeatIntervalMs = 40;
constexpr uint8_t kCommandRepeatCount = 5;
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
volatile bool mainTimerReady = false;
volatile bool mainTimerRunning = false;
uint32_t lastCrossingMs = 0;
volatile bool commandPending = false;
FinishGateProtocol::Command pendingCommand;
portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;
bool sensorSetupActive = false;
uint32_t appliedCommandId = 0;
bool buttonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t buttonChangedAtMs = 0;
uint32_t buttonPressedAtMs = 0;
bool buttonLongPressHandled = false;
bool physicalGateSetupMode = false;
bool physicalGateAwaitingClear = false;
bool gateSetRequested = false;
uint32_t gateConfiguredUntilMs = 0;
int16_t configuredGateDistanceMm = -1;
uint32_t controlBootId = 0;
uint32_t controlSequence = 0;
uint32_t nextControlSendMs = 0;
uint8_t controlRepeatsRemaining = 0;

enum class ButtonAction
{
    None,
    ShortPress,
    LongPress,
};

ButtonAction readButtonAction(uint32_t nowMs)
{
    const bool raw = digitalRead(kGateButtonPin);
    if (raw != buttonRaw)
    {
        buttonRaw = raw;
        buttonChangedAtMs = nowMs;
    }
    if (raw != buttonStable && nowMs - buttonChangedAtMs >= kButtonDebounceMs)
    {
        buttonStable = raw;
        if (buttonStable == LOW)
        {
            buttonPressedAtMs = nowMs;
            buttonLongPressHandled = false;
        }
        else if (!buttonLongPressHandled)
        {
            return ButtonAction::ShortPress;
        }
    }
    if (buttonStable == LOW && !buttonLongPressHandled &&
        nowMs - buttonPressedAtMs >= kButtonLongPressMs)
    {
        buttonLongPressHandled = true;
        return ButtonAction::LongPress;
    }
    return ButtonAction::None;
}

void requestAttemptCancellation()
{
    ++controlSequence;
    controlRepeatsRemaining = kCommandRepeatCount;
    nextControlSendMs = 0;
    Serial.println("Attempt cancellation requested by finish-gate button.");
}

void serviceControlCommand(uint32_t nowMs)
{
    if (!espNowReady || controlRepeatsRemaining == 0 ||
        (nextControlSendMs != 0 && nowMs < nextControlSendMs))
    {
        return;
    }
    GateControlProtocol::Command command;
    command.bootId = controlBootId;
    command.sequence = controlSequence;
    esp_now_send(kBroadcastAddress,
                 reinterpret_cast<const uint8_t *>(&command), sizeof(command));
    --controlRepeatsRemaining;
    nextControlSendMs = nowMs + kCommandRepeatIntervalMs;
}

void updateIndicators(uint32_t nowMs)
{
    const bool mainConnected = lastMainPacketMs != 0 &&
                               nowMs - lastMainPacketMs <= Config::kFinishGateTimeoutMs;
    const bool crossingShown = lastCrossingMs != 0 &&
                               nowMs - lastCrossingMs <= kCrossingNoticeMs;
    digitalWrite(kYellowLedPin, crossingShown ? HIGH : LOW);
    digitalWrite(kBlueLedPin,
                 !crossingShown && mainConnected && mainTimerRunning ? HIGH : LOW);
    digitalWrite(kGreenLedPin,
                 !crossingShown && mainConnected && mainTimerReady &&
                         gateDetector.isCalibrated() && !sensorSetupActive
                     ? HIGH
                     : LOW);
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
        mainTimerReady = strcmp(frame.state, "READY") == 0;
        mainTimerRunning = strcmp(frame.state, "CLEAR GATE") == 0 ||
                           strcmp(frame.state, "RUNNING") == 0;
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
        if (!physicalGateSetupMode && !physicalGateAwaitingClear)
        {
            sensorSetupActive = command.value != 0;
        }
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
    else if (configuredGateDistanceMm > 0 && nowMs < gateConfiguredUntilMs)
    {
        oled.setTextSize(1);
        oled.println("GATE SET");
    }
    else if (physicalGateSetupMode)
    {
        oled.setTextSize(1);
        oled.println("GATE SETUP - PRESS");
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
    if (configuredGateDistanceMm > 0 && nowMs < gateConfiguredUntilMs)
    {
        oled.print(configuredGateDistanceMm);
        oled.print("mm");
    }
    else if (tofReady && distanceMm >= 0)
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
    oled.display();
}
} // namespace

void setup()
{
    Serial.begin(115200);
    pinMode(kGateButtonPin, INPUT_PULLUP);
    buttonRaw = digitalRead(kGateButtonPin);
    buttonStable = buttonRaw;
    buttonChangedAtMs = millis();
    buttonPressedAtMs = buttonStable == LOW ? buttonChangedAtMs : 0;
    pinMode(kBlueLedPin, OUTPUT);
    pinMode(kYellowLedPin, OUTPUT);
    pinMode(kGreenLedPin, OUTPUT);
    digitalWrite(kBlueLedPin, LOW);
    digitalWrite(kYellowLedPin, LOW);
    digitalWrite(kGreenLedPin, LOW);
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
    controlBootId = esp_random();
    beginEspNow();
}

void loop()
{
    static uint32_t lastDisplayRefreshMs = 0;
    const uint32_t nowMs = millis();
    applyPendingCommand();

    const ButtonAction buttonAction = readButtonAction(nowMs);
    if (buttonAction == ButtonAction::ShortPress)
    {
        if (physicalGateSetupMode)
        {
            gateSetRequested = true;
        }
        else
        {
            requestAttemptCancellation();
        }
    }
    else if (buttonAction == ButtonAction::LongPress)
    {
        const bool mainConnected = lastMainPacketMs != 0 &&
                                   nowMs - lastMainPacketMs <= Config::kFinishGateTimeoutMs;
        if (!sensorSetupActive && gateDetector.isCalibrated() &&
            mainConnected && mainTimerReady)
        {
            physicalGateSetupMode = true;
            sensorSetupActive = true;
            configuredGateDistanceMm = -1;
            Serial.println("Finish gate setup mode entered. Short press confirms distance.");
        }
        else
        {
            Serial.println("Finish gate setup rejected: main gate is not READY.");
        }
    }
    serviceControlCommand(nowMs);

    int16_t measuredDistanceMm = -1;
    if (tofReady && tof.readDistance(measuredDistanceMm))
    {
        distanceMm = measuredDistanceMm;
        const GateEvent gateEvent = gateDetector.update(distanceMm, nowMs);
        if (gateSetRequested)
        {
            gateSetRequested = false;
            if (gateDetector.setTriggerFromReferenceDistanceMm(
                    gateDetector.filteredDistanceMm()))
            {
                configuredGateDistanceMm = gateDetector.triggerDistanceMm();
                gateConfiguredUntilMs = nowMs + kGateConfiguredMessageMs;
                physicalGateSetupMode = false;
                physicalGateAwaitingClear = true;
                Serial.print("Finish gate configured by button: ");
                Serial.print(configuredGateDistanceMm);
                Serial.println(" mm. Waiting for gate clear.");
            }
        }
        if (!sensorSetupActive && gateEvent == GateEvent::CrossingConfirmed)
        {
            ++crossingCount;
            lastCrossingMs = nowMs;
            Serial.print("Finish crossing #");
            Serial.println(crossingCount);
        }
        if (physicalGateAwaitingClear)
        {
            const int16_t clearThresholdMm =
                gateDetector.triggerDistanceMm() + Config::kGateApproachMm;
            const bool clearlyReleased = distanceMm < 0 || distanceMm > clearThresholdMm;
            if (gateEvent == GateEvent::GateCleared ||
                (nowMs >= gateConfiguredUntilMs && clearlyReleased))
            {
                physicalGateAwaitingClear = false;
                sensorSetupActive = false;
                Serial.println("Finish gate clear after setup. Crossings enabled.");
            }
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
    updateIndicators(nowMs);
    delay(2);
}

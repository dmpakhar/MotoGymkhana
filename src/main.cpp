#include <Arduino.h>

#include <WiFi.h>

#include "config/Config.h"
#include "config/Version.h"
#include "display/Display.h"
#include "gate/GateDetector.h"
#include "timer/SingleGateTimer.h"
#include "tof/ToFSensor.h"
#include "web/MotoWebServer.h"

namespace
{
Display display;
ToFSensor tof;
GateDetector gateDetector;
SingleGateTimer timer;
MotoWebServer webServer;

#ifdef GYMIO_MAIN_S3_MINI
constexpr uint8_t kCancelButtonPin = 8;
constexpr uint8_t kBlueLedPin = 9;
constexpr uint8_t kYellowLedPin = 12;
constexpr uint8_t kGreenLedPin = 13;
constexpr uint32_t kButtonDebounceMs = 40;
constexpr uint32_t kButtonLongPressMs = 1500;
constexpr uint32_t kCrossingIndicationMs = 600;
constexpr uint32_t kGateConfiguredMessageMs = 3000;
enum class ButtonAction
{
    None,
    ShortPress,
    LongPress,
};
bool buttonRaw = HIGH;
bool buttonStable = HIGH;
uint32_t buttonChangedAtMs = 0;
uint32_t buttonPressedAtMs = 0;
bool buttonLongPressHandled = false;
uint32_t crossingIndicationUntilMs = 0;
uint32_t gateConfiguredUntilMs = 0;
int16_t configuredGateDistanceMm = -1;
bool gateSetRequested = false;
bool physicalGateSetupMode = false;
bool physicalGateAwaitingClear = false;

void beginGateControls()
{
    pinMode(kCancelButtonPin, INPUT_PULLUP);
    buttonRaw = digitalRead(kCancelButtonPin);
    buttonStable = buttonRaw;
    buttonChangedAtMs = millis();
    buttonPressedAtMs = buttonStable == LOW ? buttonChangedAtMs : 0;
    pinMode(kBlueLedPin, OUTPUT);
    pinMode(kYellowLedPin, OUTPUT);
    pinMode(kGreenLedPin, OUTPUT);
    digitalWrite(kBlueLedPin, LOW);
    digitalWrite(kYellowLedPin, LOW);
    digitalWrite(kGreenLedPin, LOW);
}

ButtonAction readButtonAction(uint32_t nowMs)
{
    const bool raw = digitalRead(kCancelButtonPin);
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

void showGateIndicators(TimerState state, uint32_t nowMs, bool calibrated)
{
    const bool crossingShown =
        static_cast<int32_t>(crossingIndicationUntilMs - nowMs) > 0;
    const bool running = state == TimerState::RunningAwaitingClear ||
                         state == TimerState::RunningArmed;
    digitalWrite(kYellowLedPin, crossingShown ? HIGH : LOW);
    digitalWrite(kBlueLedPin, !crossingShown && running ? HIGH : LOW);
    digitalWrite(kGreenLedPin, !crossingShown && calibrated && !physicalGateSetupMode &&
                                        !physicalGateAwaitingClear &&
                                        state == TimerState::Ready
                                    ? HIGH
                                    : LOW);
}
#endif

void haltWithError(const char *message)
{
    Serial.println(message);
    display.showError(message);

    // A failed OLED cannot display an error itself. Keep the serial error visible
    // and avoid continuing with partially initialized hardware.
    while (true)
    {
        delay(1000);
    }
}
} // namespace

void setup()
{
    Serial.begin(115200);
#ifdef GYMIO_MAIN_S3_MINI
    beginGateControls();
#endif
    delay(1000); // Allows a USB serial monitor to attach during boot.

    Serial.print("Gymio v");
    Serial.println(Version::kString);

    if (!display.begin())
    {
        haltWithError("OLED INIT FAILED!");
    }
    Serial.println("OLED OK!");

    display.showStarting();

    if (!tof.begin())
    {
        haltWithError("VL53L1X INIT FAILED!");
    }
    Serial.println("VL53L1X OK!");

    display.showReady();

    if (!webServer.begin())
    {
        Serial.println("WEB SERVER INIT FAILED!");
        display.showError("WEB SERVER ERROR");
        return;
    }

    Serial.println("Wi-Fi AP started");
    Serial.print("SSID: ");
    Serial.println(Config::kWifiAccessPointName);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    if (webServer.isMdnsStarted())
    {
        Serial.println("mDNS started");
        Serial.println("Hostname: gymio.local");
        Serial.println("Open: http://gymio.local");
    }
    else
    {
        Serial.println("mDNS initialization failed");
        Serial.print("Open: http://");
        Serial.println(WiFi.softAPIP());
    }
}

void loop()
{
    const uint32_t nowMs = millis();
    webServer.handleClient();

    bool localAttemptCancelled = false;
#ifdef GYMIO_MAIN_S3_MINI
    const ButtonAction buttonAction = readButtonAction(nowMs);
    if (buttonAction == ButtonAction::ShortPress)
    {
        if (physicalGateSetupMode)
        {
            gateSetRequested = true;
            Serial.println("Gate setup confirmation requested.");
        }
        else
        {
            localAttemptCancelled = timer.cancel();
            if (localAttemptCancelled)
            {
                webServer.cancelAttempt();
                Serial.println("Current attempt cancelled by local button.");
            }
        }
    }
    else if (buttonAction == ButtonAction::LongPress)
    {
        if (!physicalGateSetupMode && !physicalGateAwaitingClear &&
            timer.state() == TimerState::Ready && gateDetector.isCalibrated())
        {
            physicalGateSetupMode = true;
            configuredGateDistanceMm = -1;
            Serial.println("Gate setup mode entered. Short press confirms distance.");
        }
        else
        {
            Serial.println("Gate setup rejected: timer is not ready or gate is not calibrated.");
        }
    }
    showGateIndicators(timer.state(), nowMs, gateDetector.isCalibrated());
#endif

    int16_t distanceMm = 0;
    if (!tof.readDistance(distanceMm))
    {
        return;
    }

#ifdef GYMIO_MAIN_S3_MINI
    if (gateSetRequested)
    {
        gateSetRequested = false;
        // Save a filtered reference with the same safety margin used by
        // automatic calibration. A threshold copied from one raw frame would
        // be crossed immediately by ordinary static ToF jitter.
        const int16_t referenceDistanceMm = gateDetector.filteredDistanceMm();
        if (gateDetector.setTriggerFromReferenceDistanceMm(referenceDistanceMm))
        {
            configuredGateDistanceMm = gateDetector.triggerDistanceMm();
            gateConfiguredUntilMs = nowMs + kGateConfiguredMessageMs;
            physicalGateSetupMode = false;
            physicalGateAwaitingClear = true;
            Serial.print("Gate configured by button: ");
            Serial.print(configuredGateDistanceMm);
            Serial.print(" mm (filtered reference: ");
            if (referenceDistanceMm < 0)
                Serial.print("INF");
            else
                Serial.print(referenceDistanceMm);
            Serial.print(" mm)");
            Serial.println(". Waiting for gate clear.");
        }
        else
        {
            Serial.println("Gate setup rejected: current ToF distance is invalid.");
        }
    }
#endif

    const bool twoGateMode = webServer.isFinishGateConnected(nowMs);
    display.setTwoGateMode(twoGateMode);

    int16_t requestedGateLimitMm = 0;
    if (webServer.takeGateLimitUpdate(requestedGateLimitMm))
    {
        if (gateDetector.setTriggerDistanceMm(requestedGateLimitMm))
        {
            Serial.print("Gate limit set to: ");
            Serial.print(requestedGateLimitMm);
            Serial.println(" mm");
        }
        else
        {
            Serial.println("Gate limit update rejected.");
        }
    }

    const GateEvent gateEvent = gateDetector.update(
        distanceMm, nowMs, tof.signalRateKcps(), tof.ambientRateKcps());
#ifdef GYMIO_MAIN_S3_MINI
    if (!physicalGateSetupMode && !physicalGateAwaitingClear &&
        gateEvent == GateEvent::CrossingConfirmed)
    {
        crossingIndicationUntilMs = nowMs + kCrossingIndicationMs;
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
            Serial.println("Gate clear after button setup. Attempts enabled.");
        }
    }
#endif
    display.setRiderName(webServer.displayRiderName());

    if (!gateDetector.isCalibrated())
    {
        webServer.updateStatus(
            distanceMm,
            -1,
            -1,
            false,
            timer.state(),
            0,
            timer.resultMs());
        display.showCalibrating(gateDetector.calibrationSampleCount(), Config::kGateCalibrationSamples);
        return;
    }

    TimerEvent timerEvent = TimerEvent::None;
    bool attemptCancelled = localAttemptCancelled;
    if (webServer.takeAttemptCancellation())
    {
        const bool webAttemptCancelled = timer.cancel();
        attemptCancelled = attemptCancelled || webAttemptCancelled;
        if (webAttemptCancelled)
        {
            webServer.cancelAttempt();
            Serial.println("Current attempt cancelled.");
        }
    }
    bool gateSetupBlocksAttempts = false;
#ifdef GYMIO_MAIN_S3_MINI
    gateSetupBlocksAttempts = physicalGateSetupMode || physicalGateAwaitingClear;
#endif
    if (!attemptCancelled && !gateSetupBlocksAttempts &&
        !webServer.isSensorSetupActive() && !webServer.isFinishGateSetupActive())
    {
        const bool finishCrossing = webServer.takeFinishCrossing();
        bool localCrossing = gateEvent == GateEvent::CrossingConfirmed;
        if (!twoGateMode && timer.state() == TimerState::RunningArmed && distanceMm >= 0)
        {
            localCrossing = localCrossing || gateDetector.isHighConfidenceEntrySample(
                                                 distanceMm,
                                                 tof.signalRateKcps(),
                                                 tof.ambientRateKcps());
        }
        static bool previousTwoGateMode = false;
        if (twoGateMode != previousTwoGateMode)
        {
            previousTwoGateMode = twoGateMode;
            Serial.println(twoGateMode ? "Timer mode: TWO GATES"
                                       : "Timer mode: SINGLE GATE");
        }
        timerEvent = timer.update(
            localCrossing,
            !gateDetector.isOccupied(),
            nowMs,
            twoGateMode,
            finishCrossing);
    }

    if (distanceMm < 0)
    {
        Serial.print("Distance: INF, filtered: ");
    }
    else
    {
        Serial.print("Distance: ");
        Serial.print(distanceMm);
        Serial.print(" mm, filtered: ");
    }
    Serial.print(gateDetector.filteredDistanceMm());
    Serial.print(" mm, trigger: ");
    Serial.print(gateDetector.triggerDistanceMm());
    Serial.print(" mm, signal/ambient: ");
    Serial.print(tof.signalRateKcps());
    Serial.print('/');
    Serial.print(tof.ambientRateKcps());
    Serial.println(" kcps");

    if (timerEvent == TimerEvent::Started)
    {
        webServer.beginAttempt();
        Serial.println("Timer started. Waiting for gate clear.");
    }
    else if (timerEvent == TimerEvent::Finished)
    {
        webServer.recordAttempt(timer.resultMs());
        Serial.print("Timer finished: ");
        Serial.print(timer.resultMs());
        Serial.println(" ms");
    }

    const bool showGateConfiguredMessage =
#ifdef GYMIO_MAIN_S3_MINI
        configuredGateDistanceMm > 0 && nowMs < gateConfiguredUntilMs;
#else
        false;
#endif
    if (webServer.isUpdateModeActive(nowMs))
    {
        const String updateIp = webServer.stationIpAddress();
        display.showUpdateMode(
            webServer.stationSsid(), updateIp.c_str(), webServer.updateModeStatus());
    }
    else if (showGateConfiguredMessage)
    {
#ifdef GYMIO_MAIN_S3_MINI
        display.showGateConfigured(configuredGateDistanceMm);
#endif
    }
#ifdef GYMIO_MAIN_S3_MINI
    else if (physicalGateSetupMode)
    {
        display.showGateSetup(distanceMm);
    }
#endif
    else switch (timer.state())
    {
    case TimerState::Ready:
        // Keep the live display stable. GateDetector maintains a rolling
        // average while still receiving every raw sample for gate decisions.
        display.showDistance(gateDetector.filteredDistanceMm());
        break;
    case TimerState::RunningAwaitingClear:
        display.showRunning(timer.elapsedMs(nowMs), true);
        break;
    case TimerState::RunningArmed:
        display.showRunning(timer.elapsedMs(nowMs), false);
        break;
    case TimerState::Result:
        display.showResult(timer.resultMs());
        break;
    }

    webServer.updateStatus(
        distanceMm,
        gateDetector.filteredDistanceMm(),
        gateDetector.triggerDistanceMm(),
        true,
        timer.state(),
        timer.elapsedMs(nowMs),
        timer.resultMs());
#ifdef GYMIO_MAIN_S3_MINI
    showGateIndicators(timer.state(), nowMs, gateDetector.isCalibrated());
#endif
}

#include <Arduino.h>
#include <WiFi.h>

#include "config/Config.h"
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
    delay(1000); // Allows a USB serial monitor to attach during boot.

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
    webServer.handleClient();

    int16_t distanceMm = 0;
    if (!tof.readDistance(distanceMm))
    {
        return;
    }

    const uint32_t nowMs = millis();

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

    const GateEvent gateEvent = gateDetector.update(distanceMm, nowMs);
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
    if (!webServer.isSensorSetupActive())
    {
        timerEvent = timer.update(
            gateEvent == GateEvent::CrossingConfirmed,
            gateEvent == GateEvent::GateCleared,
            nowMs);
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
    Serial.println(" mm");

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

    switch (timer.state())
    {
    case TimerState::Ready:
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
}

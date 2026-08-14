#if defined(GYMIO_TOF_OLED_DIAGNOSTIC)

#include <Arduino.h>

#include "display/Display.h"
#include "tof/ToFSensor.h"

namespace
{
Display display;
ToFSensor tof;
uint32_t lastReportMs = 0;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("GYMIO TOF + OLED DIAGNOSTIC");
    if (!display.begin())
    {
        Serial.println("OLED INIT FAILED");
        return;
    }
    Serial.println("OLED OK");

    if (!tof.begin())
    {
        Serial.println("VL53L0X INIT FAILED");
        display.showError("TOF INIT FAILED");
        return;
    }
    Serial.println("VL53L0X OK");
    display.showReady();
}

void loop()
{
    int16_t distanceMm = -1;
    if (!tof.readDistance(distanceMm))
    {
        return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - lastReportMs < 250)
    {
        return;
    }

    lastReportMs = nowMs;
    display.showDistance(distanceMm);

    if (distanceMm < 0)
    {
        Serial.print("Distance: INF, raw: ");
        Serial.print(tof.lastRawDistanceMm());
        Serial.print(" mm, status: ");
        Serial.println(tof.lastRangeStatus());
        return;
    }

    Serial.print("Distance: ");
    Serial.print(distanceMm);
    Serial.print(" mm, status: ");
    Serial.println(tof.lastRangeStatus());
}

#endif

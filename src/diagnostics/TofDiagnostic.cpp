#if defined(GYMIO_TOF_L0_DIAGNOSTIC)

#include <Arduino.h>

#include "tof/ToFSensor.h"

namespace
{
ToFSensor tof;
uint32_t lastReportMs = 0;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("GYMIO TOF DIAGNOSTIC");
    if (!tof.begin())
    {
        Serial.println("VL53L0X INIT FAILED");
        return;
    }

    Serial.println("VL53L0X OK");
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
    if (distanceMm < 0)
    {
        Serial.println("Distance: INF");
        return;
    }

    Serial.print("Distance: ");
    Serial.print(distanceMm);
    Serial.println(" mm");
}

#endif

#include "ToFSensor.h"

#include <Wire.h>

#include "config/Config.h"

bool ToFSensor::begin()
{
    // Both gate profiles keep VL53L1X on a dedicated hardware I2C controller.
    // Wire is already used by the OLED on GPIO10/GPIO11.
    TwoWire &tofWire = Wire1;

    tofWire.begin(Config::kTofSdaPin, Config::kTofSclPin);

    if (!sensor_.begin(Config::kTofAddress, &tofWire))
    {
        return false;
    }

    sensor_.startRanging();
    sensor_.setTimingBudget(Config::kTofTimingBudgetMs);
    return true;
}

bool ToFSensor::readDistance(int16_t &distanceMm)
{
    if (!sensor_.dataReady())
    {
        return false;
    }

    distanceMm = sensor_.distance();
    sensor_.clearInterrupt();
    return true;
}

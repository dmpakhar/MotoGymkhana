#include "ToFSensor.h"

#include <Wire.h>

#include "config/Config.h"

bool ToFSensor::begin()
{
    Wire1.begin(Config::kTofSdaPin, Config::kTofSclPin);

    if (!sensor_.begin(Config::kTofAddress, &Wire1))
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

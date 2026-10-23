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
    uint16_t signalRate = 0;
    uint16_t ambientRate = 0;
    if (distanceMm >= 0 &&
        sensor_.VL53L1X_GetSignalRate(&signalRate) == VL53L1X_ERROR_NONE &&
        sensor_.VL53L1X_GetAmbientRate(&ambientRate) == VL53L1X_ERROR_NONE)
    {
        signalRateKcps_ = signalRate;
        ambientRateKcps_ = ambientRate;
    }
    else
    {
        signalRateKcps_ = 0;
        ambientRateKcps_ = 0;
    }
    sensor_.clearInterrupt();
    return true;
}

uint16_t ToFSensor::signalRateKcps() const
{
    return signalRateKcps_;
}

uint16_t ToFSensor::ambientRateKcps() const
{
    return ambientRateKcps_;
}

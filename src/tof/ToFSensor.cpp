#include "ToFSensor.h"

#include <Wire.h>

#include "config/Config.h"

bool ToFSensor::begin()
{
#if defined(GYMIO_TARGET_ESP32_S3)
    // Existing S3 wiring keeps the sensor on a dedicated hardware I2C bus.
    TwoWire &tofWire = Wire1;
#else
    // C3 shares its single I2C bus with the OLED; device addresses differ.
    TwoWire &tofWire = Wire;
#endif

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

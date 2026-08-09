#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L1X.h>

class ToFSensor
{
public:
    bool begin();
    bool readDistance(int16_t &distanceMm);

private:
    Adafruit_VL53L1X sensor_;
};

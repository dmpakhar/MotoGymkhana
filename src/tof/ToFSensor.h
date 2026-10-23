#pragma once

#include <Arduino.h>
#include <Adafruit_VL53L1X.h>

class ToFSensor
{
public:
    bool begin();
    bool readDistance(int16_t &distanceMm);
    uint16_t signalRateKcps() const;
    uint16_t ambientRateKcps() const;

private:
    // The library forwards its optional default IRQ value through an unsigned
    // parameter to a base class. Disable that inherited GPIO explicitly; GPIO1
    // and XSHUT are intentionally not connected on this module.
    class Sensor final : public Adafruit_VL53L1X
    {
    public:
        Sensor()
            : Adafruit_VL53L1X()
        {
            gpio0 = -1;
        }
    };

    Sensor sensor_;
    uint16_t signalRateKcps_ = 0;
    uint16_t ambientRateKcps_ = 0;
};

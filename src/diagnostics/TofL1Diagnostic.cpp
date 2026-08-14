#if defined(GYMIO_TOF_L1_DIAGNOSTIC)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L1X.h>

namespace
{
constexpr uint8_t kTofSdaPin = 4;
constexpr uint8_t kTofSclPin = 5;
constexpr uint8_t kTofAddress = 0x29;

class Sensor final : public Adafruit_VL53L1X
{
public:
    Sensor()
        : Adafruit_VL53L1X()
    {
        // Prevent the library from treating its absent optional IRQ pin as an
        // invalid GPIO on ESP32-S3.
        gpio0 = -1;
    }
};

Sensor tof;
uint32_t lastReportMs = 0;
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("GYMIO VL53L1X DIAGNOSTIC");
    Wire1.begin(kTofSdaPin, kTofSclPin);

    if (!tof.begin(kTofAddress, &Wire1))
    {
        Serial.println("VL53L1X INIT FAILED");
        return;
    }

    tof.startRanging();
    tof.setTimingBudget(50);
    Serial.println("VL53L1X OK");
}

void loop()
{
    if (!tof.dataReady())
    {
        return;
    }

    const int16_t distanceMm = tof.distance();
    tof.clearInterrupt();

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

#pragma once

#include <Arduino.h>

namespace Config
{
#if defined(GYMIO_TARGET_ESP32_S3)
// Existing ESP32-S3 prototype wiring: two physically separate I2C buses.
constexpr uint8_t kOledSdaPin = 10;
constexpr uint8_t kOledSclPin = 11;
// Kept on a separate I2C bus so GPIO4/GPIO5 can be ruled out while
// diagnosing the original ESP32-S3 wiring.
constexpr uint8_t kTofSdaPin = 6;
constexpr uint8_t kTofSclPin = 7;
#else
// ESP32-C3 uses one shared I2C bus. The devices have different addresses.
constexpr uint8_t kOledSdaPin = 4;
constexpr uint8_t kOledSclPin = 5;
constexpr uint8_t kTofSdaPin = 4;
constexpr uint8_t kTofSclPin = 5;
#endif

constexpr uint8_t kOledAddress = 0x3C;

constexpr uint8_t kTofAddress = 0x29;
constexpr uint16_t kTofTimingBudgetMs = 50;

// The known working timer initialization: learn a clear-gate baseline, then
// trigger when a target is this much closer. Diagnostic validity checks still
// apply before a sample participates in this logic.
constexpr uint8_t kGateCalibrationSamples = 20;
constexpr uint8_t kGateFilterSamples = 5;
constexpr int16_t kGateApproachMm = 150;
// The VL53L1X reports -1 when it has no target. For gate decisions that means
// the farthest possible reading, not a near object.
constexpr int16_t kTofNoTargetDistanceMm = 4000;
constexpr uint16_t kGateConfirmMs = 150;
constexpr uint16_t kGateReleaseMs = 250;

constexpr uint32_t kResultDisplayMs = 5000;

// Local phone connection. Change the password before using the timer around
// other people; WPA2 passwords must be at least eight characters.
constexpr char kWifiAccessPointName[] = "Gymio";
constexpr char kWifiAccessPointPassword[] = "gymio123";
constexpr uint8_t kWifiAccessPointChannel = 1;
// ESP32-S3 SoftAP SDK maximum. The TFT uses ESP-NOW without associating, so it
// does not consume a SoftAP client slot or DHCP lease.
constexpr uint8_t kWifiAccessPointMaxConnections = 15;
constexpr uint16_t kDisplayBroadcastIntervalMs = 100;
constexpr char kMdnsHostname[] = "gymio";
} // namespace Config

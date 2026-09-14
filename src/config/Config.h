#pragma once

#include <Arduino.h>

namespace Config
{
// Main gate ESP32-S3 N8R8 wiring: two physically separate I2C buses.
constexpr uint8_t kOledSdaPin = 10;
constexpr uint8_t kOledSclPin = 11;
constexpr uint8_t kTofSdaPin = 6;
constexpr uint8_t kTofSclPin = 7;

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
// Entry uses raw ranging so a fast crossing is not diluted by the moving
// average. A clearly close sample may confirm immediately; readings close to
// the configured threshold still require two samples to reject edge noise.
constexpr int16_t kGateImmediateEntryMarginMm = 300;
// Once a one-gate attempt is armed, accept a fast return crossing from one
// valid sample. The smaller margin makes the finish easier to catch without
// treating readings right at the configured threshold as a finish.
constexpr int16_t kSingleGateFinishImmediateMarginMm = 100;
constexpr uint16_t kGateConfirmMs = 50;
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
constexpr uint16_t kFinishGateHeartbeatMs = 250;
constexpr uint16_t kFinishGateTimeoutMs = 1500;
constexpr char kMdnsHostname[] = "gymio";
} // namespace Config

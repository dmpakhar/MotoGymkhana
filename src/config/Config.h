#pragma once

#include <Arduino.h>

namespace Config
{
constexpr uint8_t kOledSdaPin = 8;
constexpr uint8_t kOledSclPin = 9;
constexpr uint8_t kOledAddress = 0x3C;

constexpr uint8_t kTofSdaPin = 4;
constexpr uint8_t kTofSclPin = 5;
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
constexpr char kMdnsHostname[] = "gymio";
} // namespace Config

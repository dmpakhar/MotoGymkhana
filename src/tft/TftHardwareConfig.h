#pragma once

#include <Arduino.h>

namespace TftHardwareConfig
{
#if defined(GYMIO_TFT_ESP32_C3_MINI_1)
constexpr char kProfileName[] = "ESP32-C3-MINI-1 ST7796S";
constexpr int8_t kSclkPin = 5;
constexpr int8_t kMosiPin = 6;
constexpr int8_t kCsPin = 2;
constexpr int8_t kDcPin = 4;
constexpr int8_t kResetPin = 8;
constexpr int8_t kBacklightPin = 0;
constexpr uint32_t kSpiWriteFrequencyHz = 20000000;
#else
// Existing ESP32-C3 Super Mini display profile.
constexpr char kProfileName[] = "ESP32-C3 Super Mini ST7796S";
constexpr int8_t kSclkPin = 4;
constexpr int8_t kMosiPin = 6;
constexpr int8_t kCsPin = 7;
constexpr int8_t kDcPin = 3;
constexpr int8_t kResetPin = 10;
constexpr int8_t kBacklightPin = 5;
constexpr uint32_t kSpiWriteFrequencyHz = 40000000;
#endif

constexpr int8_t kMisoPin = -1;
constexpr uint16_t kPanelWidth = 320;
constexpr uint16_t kPanelHeight = 480;
constexpr char kController[] = "ST7796S";
constexpr uint8_t kBacklightPwmChannel = 0;
constexpr uint32_t kBacklightPwmFrequencyHz = 20000;
constexpr uint8_t kBacklightPwmResolutionBits = 8;
constexpr uint8_t kDefaultBacklightPercent = 100;
} // namespace TftHardwareConfig

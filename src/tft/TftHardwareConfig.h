#pragma once

#include <Arduino.h>

namespace TftHardwareConfig
{
constexpr char kProfileName[] = "ESP32-C3-MINI-1 ST7796S";
constexpr int8_t kSclkPin = 5;
constexpr int8_t kMosiPin = 6;
constexpr int8_t kCsPin = 2;
constexpr int8_t kDcPin = 4;
constexpr int8_t kResetPin = 8;
constexpr int8_t kBacklightPin = 0;
constexpr uint32_t kSpiWriteFrequencyHz = 20000000;

constexpr int8_t kMisoPin = -1;
constexpr uint16_t kPanelWidth = 320;
constexpr uint16_t kPanelHeight = 480;
constexpr char kController[] = "ST7796S";
constexpr uint8_t kBacklightPwmChannel = 0;
constexpr uint32_t kBacklightPwmFrequencyHz = 20000;
constexpr uint8_t kBacklightPwmResolutionBits = 8;
constexpr uint8_t kDefaultBacklightPercent = 100;
} // namespace TftHardwareConfig

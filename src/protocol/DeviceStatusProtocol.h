#pragma once

#include <Arduino.h>

namespace DeviceStatusProtocol
{
constexpr uint32_t kMagic = 0x47445653U; // "GDVS"
constexpr uint8_t kVersion = 1;

enum class Device : uint8_t
{
    FinishGate = 1,
    TftDisplay = 2,
};

struct Frame
{
    uint32_t magic = kMagic;
    uint8_t version = kVersion;
    Device device = Device::FinishGate;
    uint16_t reserved = 0;
    uint32_t bootId = 0;
    char firmwareVersion[12]{};
};

static_assert(sizeof(Frame) <= 250, "ESP-NOW device status exceeds payload limit");
} // namespace DeviceStatusProtocol

#pragma once

#include <Arduino.h>

namespace OtaControlProtocol
{
constexpr uint32_t kMagic = 0x474F5441U; // "GOTA"
constexpr uint8_t kVersion = 1;

enum class Target : uint8_t
{
    FinishGate = 1,
    TftDisplay = 2,
};

struct Command
{
    uint32_t magic = kMagic;
    uint8_t version = kVersion;
    Target target = Target::FinishGate;
    uint16_t reserved = 0;
    uint32_t commandId = 0;
};

static_assert(sizeof(Command) <= 250, "ESP-NOW OTA command exceeds payload limit");
} // namespace OtaControlProtocol

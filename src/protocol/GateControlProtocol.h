#pragma once

#include <Arduino.h>

namespace GateControlProtocol
{
constexpr uint32_t kMagic = 0x4743544CU; // "GCTL"
constexpr uint8_t kVersion = 1;

enum class CommandType : uint8_t
{
    CancelAttempt = 1,
};

struct Command
{
    uint32_t magic = kMagic;
    uint8_t version = kVersion;
    CommandType type = CommandType::CancelAttempt;
    uint16_t reserved = 0;
    uint32_t bootId = 0;
    uint32_t sequence = 0;
};

static_assert(sizeof(Command) <= 250, "ESP-NOW control command exceeds payload limit");
} // namespace GateControlProtocol

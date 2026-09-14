#pragma once

#include <Arduino.h>

namespace FinishGateProtocol
{
constexpr uint32_t kMagic = 0x4746494EU; // "GFIN"
constexpr uint8_t kVersion = 1;

enum Flags : uint8_t
{
    SensorReady = 1U << 0,
    Calibrated = 1U << 1,
    SetupActive = 1U << 2,
};

struct Frame
{
    uint32_t magic = kMagic;
    uint8_t version = kVersion;
    uint8_t flags = 0;
    uint16_t batteryMv = 0;
    uint32_t bootId = 0;
    uint32_t sequence = 0;
    uint32_t crossingCount = 0;
    uint32_t appliedCommandId = 0;
    int16_t distanceMm = -1;
    int16_t filteredDistanceMm = -1;
    int16_t triggerDistanceMm = -1;
};

constexpr uint32_t kCommandMagic = 0x4746434DU; // "GFCM"

enum class CommandType : uint8_t
{
    SetSetupActive = 1,
    SetTriggerDistance = 2,
    UseCurrentDistance = 3,
};

struct Command
{
    uint32_t magic = kCommandMagic;
    uint8_t version = kVersion;
    CommandType type = CommandType::SetSetupActive;
    uint16_t reserved = 0;
    uint32_t commandId = 0;
    int16_t value = 0;
};

static_assert(sizeof(Frame) <= 250, "ESP-NOW frame exceeds payload limit");
static_assert(sizeof(Command) <= 250, "ESP-NOW command exceeds payload limit");
} // namespace FinishGateProtocol

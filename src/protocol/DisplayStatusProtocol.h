#pragma once

#include <Arduino.h>

namespace DisplayStatusProtocol
{
constexpr uint32_t kMagic = 0x474D494FU; // "GMIO"
constexpr uint8_t kVersion = 1;

struct Frame
{
    uint32_t magic = kMagic;
    uint8_t version = kVersion;
    uint8_t reserved[3]{};
    uint32_t sequence = 0;
    uint32_t timeMs = 0;
    uint32_t absoluteBestMs = 0;
    char state[16]{};
    char riderName[25]{};
    char absoluteBestRider[25]{};
};

static_assert(sizeof(Frame) <= 250, "ESP-NOW frame exceeds the payload limit");
} // namespace DisplayStatusProtocol

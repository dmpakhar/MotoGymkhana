#pragma once

#include <Arduino.h>

struct Rider
{
    uint32_t id = 0;
    char name[25]{};
    char clientId[41]{};
    uint32_t sessionBestMs = 0;
};

class RiderRegistry
{
public:
    static constexpr uint8_t kMaximumRiders = 20;

    RiderRegistry();

    bool add(const char *name, const char *clientId, uint32_t &newId);
    bool select(uint32_t id);
    bool remove(uint32_t id);
    void recordResult(uint32_t riderId, uint32_t durationMs);

    const Rider &selected() const;
    const Rider &find(uint32_t id) const;
    const Rider &findByClientId(const char *clientId) const;
    const Rider &at(uint8_t index) const;
    uint8_t count() const;

private:
    Rider riders_[kMaximumRiders]{};
    Rider anonymous_{};
    uint8_t count_ = 0;
    uint32_t selectedId_ = 0;
    uint32_t nextId_ = 1;
};

#include "RiderRegistry.h"

#include <ctype.h>
#include <string.h>

RiderRegistry::RiderRegistry()
{
    strcpy(anonymous_.name, "Anonymous Rider");
}

bool RiderRegistry::add(const char *name, uint32_t &newId)
{
    if (name == nullptr || count_ == kMaximumRiders)
    {
        return false;
    }

    char normalizedName[25]{};
    uint8_t length = 0;
    bool previousWasSpace = true;

    for (const char *character = name; *character != '\0'; ++character)
    {
        const unsigned char value = static_cast<unsigned char>(*character);
        const bool allowed = isalnum(value) || value == ' ' || value == '-' || value == '_';
        if (!allowed)
        {
            return false;
        }

        if (value == ' ')
        {
            if (previousWasSpace)
            {
                continue;
            }
            previousWasSpace = true;
        }
        else
        {
            previousWasSpace = false;
        }

        if (length == sizeof(normalizedName) - 1)
        {
            return false;
        }
        normalizedName[length++] = static_cast<char>(value);
    }

    while (length > 0 && normalizedName[length - 1] == ' ')
    {
        normalizedName[--length] = '\0';
    }
    if (length == 0)
    {
        return false;
    }

    Rider &rider = riders_[count_++];
    rider.id = nextId_++;
    strcpy(rider.name, normalizedName);
    newId = rider.id;
    return true;
}

bool RiderRegistry::select(uint32_t id)
{
    if (id == 0)
    {
        selectedId_ = 0;
        return true;
    }

    for (uint8_t i = 0; i < count_; ++i)
    {
        if (riders_[i].id == id)
        {
            selectedId_ = id;
            return true;
        }
    }
    return false;
}

bool RiderRegistry::remove(uint32_t id)
{
    if (id == 0)
    {
        return false;
    }

    for (uint8_t i = 0; i < count_; ++i)
    {
        if (riders_[i].id != id)
        {
            continue;
        }

        for (uint8_t next = i + 1; next < count_; ++next)
        {
            riders_[next - 1] = riders_[next];
        }
        riders_[--count_] = {};

        if (selectedId_ == id)
        {
            selectedId_ = 0;
        }
        return true;
    }

    return false;
}

void RiderRegistry::recordResult(uint32_t riderId, uint32_t durationMs)
{
    Rider *rider = &anonymous_;
    if (riderId != 0)
    {
        for (uint8_t i = 0; i < count_; ++i)
        {
            if (riders_[i].id == riderId)
            {
                rider = &riders_[i];
                break;
            }
        }
    }

    if (rider->sessionBestMs == 0 || durationMs < rider->sessionBestMs)
    {
        rider->sessionBestMs = durationMs;
    }
}

const Rider &RiderRegistry::selected() const
{
    return find(selectedId_);
}

const Rider &RiderRegistry::find(uint32_t id) const
{
    if (id != 0)
    {
        for (uint8_t i = 0; i < count_; ++i)
        {
            if (riders_[i].id == id)
            {
                return riders_[i];
            }
        }
    }
    return anonymous_;
}

const Rider &RiderRegistry::at(uint8_t index) const
{
    return riders_[index];
}

uint8_t RiderRegistry::count() const
{
    return count_;
}

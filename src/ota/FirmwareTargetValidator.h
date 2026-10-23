#pragma once

#include <Arduino.h>

class FirmwareTargetValidator
{
public:
    explicit FirmwareTargetValidator(const char *expectedMarker)
        : expectedMarker_(expectedMarker), markerLength_(strlen(expectedMarker))
    {
    }

    void reset()
    {
        matchedLength_ = 0;
        found_ = false;
    }

    void consume(const uint8_t *data, size_t length)
    {
        if (found_)
            return;
        for (size_t i = 0; i < length; ++i)
        {
            const char value = static_cast<char>(data[i]);
            if (value == expectedMarker_[matchedLength_])
            {
                if (++matchedLength_ == markerLength_)
                {
                    found_ = true;
                    return;
                }
            }
            else
            {
                matchedLength_ = value == expectedMarker_[0] ? 1U : 0U;
            }
        }
    }

    bool found() const { return found_; }

private:
    const char *expectedMarker_;
    size_t markerLength_;
    size_t matchedLength_ = 0;
    bool found_ = false;
};

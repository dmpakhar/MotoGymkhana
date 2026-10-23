#pragma once

#include <Arduino.h>

#include "config/Config.h"

enum class GateEvent
{
    None,
    CrossingConfirmed,
    GateCleared,
};

class GateDetector
{
public:
    GateEvent update(int16_t distanceMm, uint32_t nowMs,
                     uint16_t signalRateKcps = 0, uint16_t ambientRateKcps = 0);
    bool isHighConfidenceEntrySample(int16_t distanceMm,
                                     uint16_t signalRateKcps,
                                     uint16_t ambientRateKcps) const;
    bool setTriggerDistanceMm(int16_t triggerDistanceMm);
    bool setTriggerFromReferenceDistanceMm(int16_t referenceDistanceMm);

    bool isCalibrated() const;
    bool isOccupied() const;
    uint8_t calibrationSampleCount() const;
    int16_t filteredDistanceMm() const;
    int16_t triggerDistanceMm() const;

private:
    int16_t samples_[5]{};
    uint8_t sampleCount_ = 0;
    uint8_t nextSampleIndex_ = 0;
    int32_t sampleSum_ = 0;
    int16_t calibrationSamples_[Config::kGateCalibrationSamples]{};
    uint8_t calibrationCount_ = 0;
    int16_t triggerDistanceMm_ = -1;
    bool calibrated_ = false;

    bool gateOccupied_ = false;
    bool candidateActive_ = false;
    uint32_t candidateSinceMs_ = 0;
};

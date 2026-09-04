#include "GateDetector.h"

#include "config/Config.h"

GateEvent GateDetector::update(int16_t distanceMm, uint32_t nowMs)
{
    const int16_t normalizedDistanceMm = distanceMm < 0 ? Config::kTofNoTargetDistanceMm : distanceMm;

    if (!calibrated_)
    {
        calibrationSum_ += normalizedDistanceMm;
        ++calibrationCount_;
        if (calibrationCount_ == Config::kGateCalibrationSamples)
        {
            triggerDistanceMm_ = calibrationSum_ / calibrationCount_ - Config::kGateApproachMm;
            calibrated_ = true;
        }
        return GateEvent::None;
    }

    if (sampleCount_ < Config::kGateFilterSamples)
    {
        samples_[sampleCount_++] = normalizedDistanceMm;
        sampleSum_ += normalizedDistanceMm;
    }
    else
    {
        sampleSum_ -= samples_[nextSampleIndex_];
        samples_[nextSampleIndex_] = normalizedDistanceMm;
        sampleSum_ += normalizedDistanceMm;
        nextSampleIndex_ = (nextSampleIndex_ + 1) % Config::kGateFilterSamples;
    }

    // Entry must react to a short, fast crossing. Using the five-sample average
    // here can hide two genuine close readings among far background samples.
    // Release remains filtered to prevent a single far/INF sample from arming
    // the finish while the rider is still occupying the gate.
    const int16_t decisionDistanceMm = gateOccupied_
                                           ? filteredDistanceMm()
                                           : normalizedDistanceMm;
    const bool objectPresent = decisionDistanceMm <= triggerDistanceMm_;

    // A fast motorcycle may cross the sensor beam within one 50 ms ranging
    // frame. The VL53L1X driver has already rejected invalid/range-error
    // measurements before they reach this detector, so accept one sample when
    // it is comfortably inside the gate threshold. Keep normal confirmation
    // for marginal readings near the threshold.
    const int16_t immediateEntryThresholdMm =
        triggerDistanceMm_ > Config::kGateImmediateEntryMarginMm
            ? triggerDistanceMm_ - Config::kGateImmediateEntryMarginMm
            : 0;
    if (!gateOccupied_ && normalizedDistanceMm <= immediateEntryThresholdMm)
    {
        gateOccupied_ = true;
        candidateActive_ = false;
        return GateEvent::CrossingConfirmed;
    }

    const uint16_t requiredStableMs = gateOccupied_ ? Config::kGateReleaseMs : Config::kGateConfirmMs;
    const bool stateWouldChange = objectPresent != gateOccupied_;

    if (!stateWouldChange)
    {
        candidateActive_ = false;
        return GateEvent::None;
    }

    if (!candidateActive_)
    {
        candidateActive_ = true;
        candidateSinceMs_ = nowMs;
        return GateEvent::None;
    }

    if (nowMs - candidateSinceMs_ < requiredStableMs)
    {
        return GateEvent::None;
    }

    gateOccupied_ = objectPresent;
    candidateActive_ = false;
    return gateOccupied_ ? GateEvent::CrossingConfirmed : GateEvent::GateCleared;
}

bool GateDetector::setTriggerDistanceMm(int16_t triggerDistanceMm)
{
    if (!calibrated_ || triggerDistanceMm <= 0 || triggerDistanceMm >= Config::kTofNoTargetDistanceMm)
    {
        return false;
    }

    triggerDistanceMm_ = triggerDistanceMm;
    return true;
}

bool GateDetector::isCalibrated() const
{
    return calibrated_;
}

uint8_t GateDetector::calibrationSampleCount() const
{
    return calibrationCount_;
}

int16_t GateDetector::filteredDistanceMm() const
{
    return sampleCount_ == 0 ? -1 : sampleSum_ / sampleCount_;
}

int16_t GateDetector::triggerDistanceMm() const
{
    return triggerDistanceMm_;
}

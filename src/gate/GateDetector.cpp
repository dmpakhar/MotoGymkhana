#include "GateDetector.h"

#include "config/Config.h"

GateEvent GateDetector::update(int16_t distanceMm, uint32_t nowMs,
                               uint16_t signalRateKcps, uint16_t ambientRateKcps)
{
    const int16_t normalizedDistanceMm = distanceMm < 0 ? Config::kTofNoTargetDistanceMm : distanceMm;

    if (!calibrated_)
    {
        calibrationSamples_[calibrationCount_++] = normalizedDistanceMm;
        if (calibrationCount_ == Config::kGateCalibrationSamples)
        {
            int16_t sorted[Config::kGateCalibrationSamples];
            memcpy(sorted, calibrationSamples_, sizeof(sorted));
            for (uint8_t i = 1; i < Config::kGateCalibrationSamples; ++i)
            {
                const int16_t value = sorted[i];
                int8_t j = static_cast<int8_t>(i) - 1;
                while (j >= 0 && sorted[j] > value)
                {
                    sorted[j + 1] = sorted[j];
                    --j;
                }
                sorted[j + 1] = value;
            }
            constexpr uint8_t trim = Config::kGateCalibrationSamples / 4;
            int32_t trimmedSum = 0;
            for (uint8_t i = trim; i < Config::kGateCalibrationSamples - trim; ++i)
                trimmedSum += sorted[i];
            constexpr uint8_t retained = Config::kGateCalibrationSamples - 2 * trim;
            triggerDistanceMm_ = trimmedSum / retained - Config::kGateApproachMm;
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

    // A fast motorcycle may cross the sensor beam within one 20 ms ranging
    // frame. The VL53L1X driver has already rejected invalid/range-error
    // measurements before they reach this detector, so accept one sample when
    // it is comfortably inside the gate threshold. Keep normal confirmation
    // for marginal readings near the threshold.
    if (!gateOccupied_ &&
        isHighConfidenceEntrySample(normalizedDistanceMm, signalRateKcps, ambientRateKcps))
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

bool GateDetector::isHighConfidenceEntrySample(int16_t distanceMm,
                                                uint16_t signalRateKcps,
                                                uint16_t ambientRateKcps) const
{
    if (!calibrated_ || distanceMm < 0)
        return false;
    const int16_t immediateEntryThresholdMm =
        triggerDistanceMm_ > Config::kGateImmediateEntryMarginMm
            ? triggerDistanceMm_ - Config::kGateImmediateEntryMarginMm
            : 0;
    const uint32_t requiredForAmbient =
        static_cast<uint32_t>(ambientRateKcps) * Config::kGateImmediateSignalAmbientRatio;
    return distanceMm <= immediateEntryThresholdMm &&
           signalRateKcps >= Config::kGateImmediateMinSignalKcps &&
           static_cast<uint32_t>(signalRateKcps) >= requiredForAmbient;
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

bool GateDetector::setTriggerFromReferenceDistanceMm(int16_t referenceDistanceMm)
{
    const int16_t normalizedReferenceMm =
        referenceDistanceMm < 0 ? Config::kTofNoTargetDistanceMm : referenceDistanceMm;
    if (normalizedReferenceMm <= Config::kGateApproachMm)
    {
        return false;
    }

    // Keep the trigger safely inside the measured clear/background distance.
    // Saving the reference itself would let ordinary ToF jitter immediately
    // oscillate across the threshold and create false crossings.
    return setTriggerDistanceMm(normalizedReferenceMm - Config::kGateApproachMm);
}

bool GateDetector::isCalibrated() const
{
    return calibrated_;
}

bool GateDetector::isOccupied() const
{
    return gateOccupied_;
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

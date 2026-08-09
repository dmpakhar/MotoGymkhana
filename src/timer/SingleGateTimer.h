#pragma once

#include <Arduino.h>

enum class TimerState
{
    Ready,
    RunningAwaitingClear,
    RunningArmed,
    Result,
};

enum class TimerEvent
{
    None,
    Started,
    Finished,
    ResetToReady,
};

class SingleGateTimer
{
public:
    TimerEvent update(bool crossingConfirmed, bool gateCleared, uint32_t nowMs);

    TimerState state() const;
    uint32_t elapsedMs(uint32_t nowMs) const;
    uint32_t resultMs() const;

private:
    TimerState state_ = TimerState::Ready;
    uint32_t startedAtMs_ = 0;
    uint32_t resultMs_ = 0;
    uint32_t resultShownAtMs_ = 0;
};

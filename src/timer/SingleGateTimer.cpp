#include "SingleGateTimer.h"

#include "config/Config.h"

TimerEvent SingleGateTimer::update(bool crossingConfirmed, bool gateCleared, uint32_t nowMs)
{
    switch (state_)
    {
    case TimerState::Ready:
        if (crossingConfirmed)
        {
            startedAtMs_ = nowMs; // Capture the confirmed crossing timestamp.
            state_ = TimerState::RunningAwaitingClear;
            return TimerEvent::Started;
        }
        break;

    case TimerState::RunningAwaitingClear:
        if (gateCleared)
        {
            state_ = TimerState::RunningArmed;
        }
        break;

    case TimerState::RunningArmed:
        if (crossingConfirmed)
        {
            resultMs_ = nowMs - startedAtMs_;
            resultShownAtMs_ = nowMs;
            state_ = TimerState::Result;
            return TimerEvent::Finished;
        }
        break;

    case TimerState::Result:
        if (nowMs - resultShownAtMs_ >= Config::kResultDisplayMs)
        {
            state_ = TimerState::Ready;
            return TimerEvent::ResetToReady;
        }
        break;
    }

    return TimerEvent::None;
}

TimerState SingleGateTimer::state() const
{
    return state_;
}

uint32_t SingleGateTimer::elapsedMs(uint32_t nowMs) const
{
    return (state_ == TimerState::RunningAwaitingClear || state_ == TimerState::RunningArmed)
               ? nowMs - startedAtMs_
               : 0;
}

uint32_t SingleGateTimer::resultMs() const
{
    return resultMs_;
}

#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "rider/RiderRegistry.h"
#include "timer/SingleGateTimer.h"

class MotoWebServer
{
public:
    bool begin();
    void handleClient();
    bool isMdnsStarted() const;
    void beginAttempt();
    void recordAttempt(uint32_t durationMs);
    bool takeGateLimitUpdate(int16_t &limitMm);
    bool isSensorSetupActive() const;
    const char *displayRiderName() const;
    void updateStatus(
        int16_t distanceMm,
        int16_t filteredDistanceMm,
        int16_t triggerDistanceMm,
        bool calibrated,
        TimerState timerState,
        uint32_t elapsedMs,
        uint32_t resultMs);

private:
    struct Status
    {
        int16_t distanceMm = 0;
        int16_t filteredDistanceMm = 0;
        int16_t triggerDistanceMm = 0;
        bool calibrated = false;
        TimerState timerState = TimerState::Ready;
        uint32_t elapsedMs = 0;
        uint32_t resultMs = 0;
    } status_;

    static constexpr uint8_t kHistoryCapacity = 20;
    struct Attempt
    {
        uint32_t number;
        uint32_t durationMs;
        Rider rider;
        bool valid;
    } history_[kHistoryCapacity]{};
    uint8_t historyCount_ = 0;
    uint8_t nextHistoryIndex_ = 0;
    uint32_t attemptNumber_ = 0;
    RiderRegistry riders_;
    Rider attemptRider_{};
    bool attemptRiderLocked_ = false;
    bool gateLimitUpdatePending_ = false;
    int16_t pendingGateLimitMm_ = -1;
    bool sensorSetupActive_ = false;
    bool mdnsStarted_ = false;

    void handleRoot();
    void handleStatus();
    void handleHistory();
    void handleInvalidateAttempt();
    void handleRiders();
    void handleAddRider();
    void handleSelectRider();
    void handleRemoveRider();
    void handleSetGateLimit();
    void handleUseCurrentGateLimit();
    void handleSensorSetup();
    const Rider &riderForStatus() const;
    uint32_t bestForRider(uint32_t riderId) const;
    const char *timerStateText() const;

    WebServer server_{80};
};

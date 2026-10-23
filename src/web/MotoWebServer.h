#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

#include "config/Config.h"
#include "rider/RiderRegistry.h"
#include "timer/SingleGateTimer.h"
#include "protocol/OtaControlProtocol.h"
#include "config/FirmwareIdentity.h"
#include "ota/FirmwareTargetValidator.h"

class MotoWebServer
{
public:
    bool begin();
    void handleClient();
    bool isMdnsStarted() const;
    void beginAttempt();
    void recordAttempt(uint32_t durationMs);
    void cancelAttempt();
    bool takeAttemptCancellation();
    bool takeGateLimitUpdate(int16_t &limitMm);
    bool isSensorSetupActive() const;
    bool isFinishGateConnected(uint32_t nowMs);
    bool isFinishGateSetupActive();
    bool takeFinishCrossing();
    const char *displayRiderName() const;
    bool isUpdateModeActive(uint32_t nowMs) const;
    const char *stationSsid() const;
    String stationIpAddress() const;
    const char *updateModeStatus() const;
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
        bool manuallyValid;
    } history_[kHistoryCapacity]{};
    uint8_t historyCount_ = 0;
    uint8_t nextHistoryIndex_ = 0;
    uint32_t attemptNumber_ = 0;
    uint32_t minimumValidTimeMs_ = 0;
    Preferences preferences_;
    RiderRegistry riders_;
    Rider attemptRider_{};
    bool attemptRiderLocked_ = false;
    bool gateLimitUpdatePending_ = false;
    int16_t pendingGateLimitMm_ = -1;
    bool sensorSetupActive_ = false;
    bool mdnsStarted_ = false;
    bool stationWasConnected_ = false;
    String stationSsid_;
    String stationPassword_;
    bool adminAuthenticationEnabled_ = Config::kAdminAuthenticationEnabledByDefault;
    uint32_t stationConnectStartedMs_ = 0;
    uint32_t stationLastAttemptMs_ = 0;
    uint32_t stationConnectScheduledAtMs_ = 0;
    uint32_t lastDisplayBroadcastMs_ = 0;
    uint32_t displaySequence_ = 0;
    bool espNowReady_ = false;
    OtaControlProtocol::Target otaTarget_ = OtaControlProtocol::Target::FinishGate;
    uint32_t otaCommandId_ = 0;
    uint32_t otaCommandUntilMs_ = 0;
    bool otaUploadStarted_ = false;
    bool otaUploadSucceeded_ = false;
    uint32_t otaRestartAtMs_ = 0;
    uint32_t updateModeUntilMs_ = 0;
    String updateModeStatus_ = "READY FOR UPDATE";
    String latestReleaseTag_;
    bool onlineUpdateAvailable_ = false;
    uint32_t onlineUpdateScheduledAtMs_ = 0;
    uint32_t remoteOnlineUpdateScheduledAtMs_ = 0;
    FirmwareTargetValidator otaTargetValidator_{FirmwareIdentity::kMarker};
    portMUX_TYPE finishGateMux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t lastFinishPacketMs_ = 0;
    volatile uint32_t lastFinishStatusPacketMs_ = 0;
    volatile uint32_t lastTftStatusPacketMs_ = 0;
    char finishFirmwareVersion_[12]{};
    char tftFirmwareVersion_[12]{};
    volatile uint32_t finishBootId_ = 0;
    volatile uint32_t finishCrossingCount_ = 0;
    volatile uint8_t finishFlags_ = 0;
    volatile int16_t finishDistanceMm_ = -1;
    volatile int16_t finishFilteredDistanceMm_ = -1;
    volatile int16_t finishTriggerDistanceMm_ = -1;
    volatile uint32_t finishAppliedCommandId_ = 0;
    volatile bool finishCrossingPending_ = false;
    uint32_t finishCommandId_ = 0;
    uint8_t finishCommandType_ = 0;
    int16_t finishCommandValue_ = 0;
    volatile bool finishCommandPending_ = false;
    volatile bool attemptCancellationPending_ = false;
    volatile uint32_t controlBootId_ = 0;
    volatile uint32_t controlSequence_ = 0;
    static MotoWebServer *instance_;
    static void receiveEspNow(const uint8_t *mac, const uint8_t *data, int length);

    void handleRoot();
    void handleRiderRoot();
    void handleOtaPage();
    void handleConfigurationPage();
    void handleConfigurationStatus();
    void handleSaveWifiConfiguration();
    void handleDisconnectWifi();
    void handleSaveSecurityConfiguration();
    void handleCheckForUpdates();
    void handleUpdateDevices();
    void handleInstallOnlineUpdate();
    void handleInstallFinishOnlineUpdate();
    void handleInstallTftOnlineUpdate();
    void handleUpdateState();
    void handlePrepareRemoteOta();
    void handleMainOtaComplete();
    void handleMainOtaUpload();
    void handleStatus();
    void handleHistory();
    void handleInvalidateAttempt();
    void handleRevalidateAttempt();
    void handleSetAttemptValidity(bool valid);
    void handleSetMinimumValidTime();
    void handleCancelAttempt();
    void handleAssignAttempt();
    void handleRiders();
    void handleAddRider();
    void handleSelectRider();
    void handlePrepareRider();
    void handleLogoutRider();
    void handleRemoveRider();
    void handleSetGateLimit();
    void handleUseCurrentGateLimit();
    void handleSensorSetup();
    void handleFinishSensorSetup();
    void handleSetFinishGateLimit();
    void handleUseCurrentFinishGateLimit();
    void sendFinishCommand(uint8_t type, int16_t value);
    void broadcastDisplayStatus();
    bool authenticateOta();
    bool authenticateAdministration();
    void startStationConnection();
    void serviceStationConnection();
    void restartMdns();
    void serviceOnlineUpdate();
    void serviceRemoteOnlineUpdate();
    const Rider &riderForStatus() const;
    bool isAttemptValid(const Attempt &attempt) const;
    bool isAttemptAutomaticallyInvalid(const Attempt &attempt) const;
    uint32_t bestForRider(uint32_t riderId) const;
    uint32_t absoluteBest() const;
    const char *absoluteBestRiderName() const;
    const char *timerStateText() const;

    WebServer server_{80};
};

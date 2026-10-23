#pragma once

#include <Arduino.h>

#include <Adafruit_SSD1306.h>

class Display
{
public:
    bool begin();
    void setRiderName(const char *name);
    void setTwoGateMode(bool enabled);
    void showStarting();
    void showCalibrating(uint8_t sampleCount, uint8_t totalSamples);
    void showReady();
    void showDistance(int16_t distanceMm);
    void showRunning(uint32_t elapsedMs, bool awaitingClear);
    void showResult(uint32_t resultMs);
    void showGateSetup(int16_t distanceMm);
    void showGateConfigured(int16_t distanceMm);
    void showUpdateMode(const char *ssid, const char *ipAddress, const char *status);
    void showError(const char *message);

private:
    void showHeader();

    Adafruit_SSD1306 oled_{128, 64, &Wire, -1};
    char riderName_[10] = "ANON";
    bool twoGateMode_ = false;
    bool initialized_ = false;
};

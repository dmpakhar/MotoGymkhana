#include "Display.h"

#include <string.h>
#include <Wire.h>

#include "config/Config.h"

bool Display::begin()
{
    Wire.begin(Config::kOledSdaPin, Config::kOledSclPin);
    initialized_ = oled_.begin(SSD1306_SWITCHCAPVCC, Config::kOledAddress);

    if (initialized_)
    {
        oled_.setTextColor(SSD1306_WHITE);
    }

    return initialized_;
}

void Display::showHeader()
{
    oled_.setTextSize(1);
    oled_.setCursor(0, 0);
    oled_.print("GYMIO");
    oled_.setCursor(70, 0);
    oled_.print(riderName_);
}

void Display::setRiderName(const char *name)
{
    if (name == nullptr || *name == '\0')
    {
        strcpy(riderName_, "ANON");
        return;
    }

    strncpy(riderName_, name, sizeof(riderName_) - 1);
    riderName_[sizeof(riderName_) - 1] = '\0';
}

void Display::showStarting()
{
    if (!initialized_)
    {
        return;
    }

    oled_.clearDisplay();
    showHeader();
    oled_.setTextSize(1);
    oled_.setCursor(35, 30);
    oled_.println("STARTING...");
    oled_.display();
}

void Display::showReady()
{
    if (!initialized_)
    {
        return;
    }

    oled_.clearDisplay();
    showHeader();
    oled_.setTextSize(1);
    oled_.setCursor(46, 48);
    oled_.println("READY");
    oled_.display();
}

void Display::showCalibrating(uint8_t sampleCount, uint8_t totalSamples)
{
    if (!initialized_)
    {
        return;
    }

    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setCursor(18, 0);
    oled_.println("GYMIO by dpakhar");
    oled_.setCursor(20, 26);
    oled_.println("CALIBRATING GATE");
    oled_.setCursor(42, 42);
    oled_.print(sampleCount);
    oled_.print("/");
    oled_.println(totalSamples);
    oled_.display();
}

void Display::showDistance(int16_t distanceMm)
{
    if (!initialized_)
    {
        return;
    }

    oled_.clearDisplay();
    showHeader();

    if (distanceMm < 0)
    {
        oled_.setTextSize(2);
        oled_.setCursor(10, 24);
        oled_.println("INF");
        oled_.setTextSize(1);
        oled_.setCursor(46, 52);
        oled_.println("READY");
        oled_.display();
        return;
    }

    oled_.setTextSize(3);
    oled_.setCursor(12, 22);
    oled_.print(distanceMm);
    oled_.setTextSize(1);
    oled_.println(" mm");

    oled_.setTextSize(1);
    oled_.setCursor(46, 52);
    oled_.println("READY");
    oled_.display();
}

void Display::showRunning(uint32_t elapsedMs, bool awaitingClear)
{
    if (!initialized_)
    {
        return;
    }

    const uint32_t minutes = elapsedMs / 60000;
    const uint32_t seconds = (elapsedMs / 1000) % 60;
    const uint32_t milliseconds = elapsedMs % 1000;

    oled_.clearDisplay();
    showHeader();
    oled_.setTextSize(2);
    oled_.setCursor(0, 22);
    oled_.printf("%02lu:%02lu.%03lu", minutes, seconds, milliseconds);
    oled_.setTextSize(1);
    oled_.setCursor(20, 52);
    oled_.println(awaitingClear ? "CLEAR GATE..." : "RUNNING");
    oled_.display();
}

void Display::showResult(uint32_t resultMs)
{
    if (!initialized_)
    {
        return;
    }

    const uint32_t minutes = resultMs / 60000;
    const uint32_t seconds = (resultMs / 1000) % 60;
    const uint32_t milliseconds = resultMs % 1000;

    oled_.clearDisplay();
    showHeader();
    oled_.setTextSize(1);
    oled_.setCursor(44, 18);
    oled_.println("RESULT");
    oled_.setTextSize(2);
    oled_.setCursor(0, 34);
    oled_.printf("%02lu:%02lu.%03lu", minutes, seconds, milliseconds);
    oled_.display();
}

void Display::showError(const char *message)
{
    if (!initialized_)
    {
        return;
    }

    oled_.clearDisplay();
    oled_.setTextSize(1);
    oled_.setCursor(0, 0);
    oled_.println("GYMIO ERROR");
    oled_.setCursor(0, 24);
    oled_.println(message);
    oled_.display();
}

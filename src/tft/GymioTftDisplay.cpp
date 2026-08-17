#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config/Config.h"
#include "protocol/DisplayStatusProtocol.h"

namespace
{
constexpr int8_t kTftSckPin = 4;
constexpr int8_t kTftMosiPin = 6;
constexpr int8_t kTftCsPin = 7;
constexpr int8_t kTftDcPin = 3;
constexpr int8_t kTftResetPin = 10;
constexpr int8_t kTftBacklightPin = 5;
constexpr uint32_t kSpiWriteFrequencyHz = 40000000;
constexpr uint8_t kBacklightPwmChannel = 0;
constexpr uint32_t kBacklightPwmFrequencyHz = 20000;
constexpr uint8_t kBacklightPwmResolutionBits = 8;
constexpr uint8_t kDefaultBacklightPercent = 100;
// Sunlight-oriented palette: maximize luminance separation on the TFT.
constexpr uint16_t kBackgroundColor = TFT_BLACK;
constexpr uint16_t kCardColor = 0x1082;
constexpr uint16_t kMutedColor = 0xCE59;
constexpr uint16_t kAccentColor = 0xDFA0;

class St7796Display final : public lgfx::LGFX_Device
{
public:
    St7796Display()
    {
        auto busConfig = bus_.config();
        busConfig.spi_host = SPI2_HOST;
        busConfig.spi_mode = 0;
        busConfig.freq_write = kSpiWriteFrequencyHz;
        busConfig.freq_read = 10000000;
        busConfig.spi_3wire = false;
        busConfig.use_lock = true;
        busConfig.dma_channel = SPI_DMA_CH_AUTO;
        busConfig.pin_sclk = kTftSckPin;
        busConfig.pin_mosi = kTftMosiPin;
        busConfig.pin_miso = -1;
        busConfig.pin_dc = kTftDcPin;
        bus_.config(busConfig);
        panel_.setBus(&bus_);

        auto panelConfig = panel_.config();
        panelConfig.pin_cs = kTftCsPin;
        panelConfig.pin_rst = kTftResetPin;
        panelConfig.pin_busy = -1;
        panelConfig.panel_width = 320;
        panelConfig.panel_height = 480;
        panelConfig.memory_width = 320;
        panelConfig.memory_height = 480;
        panelConfig.offset_x = 0;
        panelConfig.offset_y = 0;
        panelConfig.offset_rotation = 0;
        panelConfig.readable = false;
        panelConfig.invert = false;
        panelConfig.rgb_order = false;
        panelConfig.dlen_16bit = false;
        panelConfig.bus_shared = false;
        panel_.config(panelConfig);
        setPanel(&panel_);
    }

private:
    lgfx::Bus_SPI bus_;
    lgfx::Panel_ST7796 panel_;
};

St7796Display display;
lgfx::LGFX_Sprite digitSprite(&display);
uint32_t lastTimerUpdateMs = 0;
uint32_t lastStatusPacketMs = 0;
char previousTimerText[10] = "         ";
char displayedRider[25] = "NONAME";
char displayedBestRider[25] = "";
char displayedState[16] = "";
uint32_t displayedBestMs = UINT32_MAX;
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
DisplayStatusProtocol::Frame pendingFrame;
volatile bool framePending = false;
bool espNowReady = false;

void setBacklightBrightness(uint8_t percent)
{
    percent = min(percent, static_cast<uint8_t>(100));
    constexpr uint16_t kMaximumDuty = (1U << kBacklightPwmResolutionBits) - 1U;
    const uint32_t duty = (static_cast<uint32_t>(percent) * kMaximumDuty + 50U) / 100U;
    ledcWrite(kBacklightPwmChannel, duty);
}

void receiveEspNow(const uint8_t *, const uint8_t *data, int length)
{
    if (length != static_cast<int>(sizeof(DisplayStatusProtocol::Frame)))
    {
        return;
    }
    DisplayStatusProtocol::Frame frame;
    memcpy(&frame, data, sizeof(frame));
    if (frame.magic != DisplayStatusProtocol::kMagic ||
        frame.version != DisplayStatusProtocol::kVersion)
    {
        return;
    }
    portENTER_CRITICAL(&statusMux);
    pendingFrame = frame;
    framePending = true;
    portEXIT_CRITICAL(&statusMux);
}

void drawCentered(const char *text, int32_t y, const lgfx::IFont *font)
{
    display.setFont(font);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(lgfx::textdatum_t::middle_center);
    display.drawString(text, display.width() / 2, y);
}

void drawCenteredAt(const char *text, int32_t x, int32_t y, const lgfx::IFont *font, uint16_t color)
{
    display.setFont(font);
    display.setTextColor(color);
    display.setTextDatum(lgfx::textdatum_t::middle_center);
    display.drawString(text, x, y);
}

void drawGymioUi()
{
    display.fillScreen(kBackgroundColor);
    drawCenteredAt("GYMIO", 240, 22, &fonts::Font4, TFT_WHITE);
    drawCenteredAt("READY", 240, 49, &fonts::Font2, kAccentColor);

    display.fillRoundRect(16, 68, 292, 230, 18, kCardColor);
    drawCenteredAt("CURRENT TIME", 162, 105, &fonts::Font2, kMutedColor);
    drawCenteredAt("00:00.000", 162, 175, &fonts::Font4, TFT_WHITE);

    display.fillRoundRect(324, 68, 140, 105, 18, kCardColor);
    drawCenteredAt("RIDER", 394, 92, &fonts::Font2, kMutedColor);
    drawCenteredAt("NONAME", 394, 136, &fonts::Font2, TFT_WHITE);

    display.fillRoundRect(324, 190, 140, 108, 18, kCardColor);
    drawCenteredAt("BEST", 394, 214, &fonts::Font2, kMutedColor);
    drawCenteredAt("--:--.---", 394, 262, &fonts::Font2, TFT_WHITE);
}

void updateTimer(uint32_t elapsedMs)
{
    const uint32_t minutes = elapsedMs / 60000U;
    const uint32_t seconds = (elapsedMs / 1000U) % 60U;
    const uint32_t milliseconds = elapsedMs % 1000U;
    char text[16];
    snprintf(text, sizeof(text), "%02lu:%02lu.%03lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(milliseconds));

    constexpr int32_t cellWidth = 48;
    constexpr int32_t startX = 24;
    constexpr int32_t startY = 105;

    for (size_t i = 0; i < 9; ++i)
    {
        if (text[i] == previousTimerText[i])
        {
            continue;
        }
        char glyph[2] = {text[i], '\0'};
        digitSprite.fillSprite(kCardColor);
        digitSprite.setFont(&fonts::Font8);
        digitSprite.setTextColor(TFT_WHITE, kCardColor);
        digitSprite.setTextDatum(lgfx::textdatum_t::middle_center);
        digitSprite.drawString(glyph, cellWidth / 2, 45);
        digitSprite.pushSprite(startX + static_cast<int32_t>(i) * cellWidth, startY);
        previousTimerText[i] = text[i];
    }
}

void formatTime(uint32_t timeMs, char *output, size_t outputSize)
{
    snprintf(output, outputSize, "%02lu:%02lu.%03lu",
             static_cast<unsigned long>(timeMs / 60000U),
             static_cast<unsigned long>((timeMs / 1000U) % 60U),
             static_cast<unsigned long>(timeMs % 1000U));
}

void updateHeaderState(const char *state)
{
    if (strcmp(state, displayedState) == 0)
    {
        return;
    }
    strncpy(displayedState, state, sizeof(displayedState) - 1U);
    displayedState[sizeof(displayedState) - 1U] = '\0';
    display.fillRect(340, 5, 135, 48, kBackgroundColor);
    drawCenteredAt(displayedState, 408, 30, &fonts::Font2, kAccentColor);
}

void updateRider(const char *rider)
{
    const char *name = strcmp(rider, "ANON") == 0 ? "NONAME" : rider;
    if (strcmp(name, displayedRider) == 0)
    {
        return;
    }
    strncpy(displayedRider, name, sizeof(displayedRider) - 1U);
    displayedRider[sizeof(displayedRider) - 1U] = '\0';
    display.fillRect(24, 258, 200, 43, kCardColor);
    drawCenteredAt(displayedRider, 124, 279, &fonts::Font4, TFT_WHITE);
}

void updateBest(uint32_t bestMs, const char *bestRider)
{
    const char *name = strcmp(bestRider, "ANON") == 0 ? "NONAME" : bestRider;
    if (bestMs == displayedBestMs && strcmp(name, displayedBestRider) == 0)
    {
        return;
    }
    displayedBestMs = bestMs;
    strncpy(displayedBestRider, name, sizeof(displayedBestRider) - 1U);
    displayedBestRider[sizeof(displayedBestRider) - 1U] = '\0';
    char text[16] = "--:--.---";
    if (bestMs != 0U)
    {
        formatTime(bestMs, text, sizeof(text));
    }
    display.fillRect(256, 253, 200, 49, kCardColor);
    drawCenteredAt(text, 356, 269, &fonts::Font4, TFT_WHITE);
    drawCenteredAt(bestMs == 0U ? "" : displayedBestRider,
                   356, 295, &fonts::Font0, kMutedColor);
}

void receiveStatus(uint32_t nowMs)
{
    if (!framePending)
    {
        return;
    }
    DisplayStatusProtocol::Frame frame;
    portENTER_CRITICAL(&statusMux);
    frame = pendingFrame;
    framePending = false;
    portEXIT_CRITICAL(&statusMux);

    frame.state[sizeof(frame.state) - 1U] = '\0';
    frame.riderName[sizeof(frame.riderName) - 1U] = '\0';
    frame.absoluteBestRider[sizeof(frame.absoluteBestRider) - 1U] = '\0';
    lastStatusPacketMs = nowMs;
    updateHeaderState(frame.state);
    updateTimer(frame.timeMs);
    updateRider(frame.riderName);
    updateBest(frame.absoluteBestMs, frame.absoluteBestRider);
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("GYMIO ESP32-C3 TFT DISPLAY");
    ledcSetup(kBacklightPwmChannel, kBacklightPwmFrequencyHz, kBacklightPwmResolutionBits);
    ledcAttachPin(kTftBacklightPin, kBacklightPwmChannel);
    setBacklightBrightness(kDefaultBacklightPercent);
    Serial.printf("TFT backlight: %u%%\n", kDefaultBacklightPercent);
    Serial.println("Initializing display controller at 40 MHz...");

    if (!display.init())
    {
        Serial.println("TFT INIT FAILED");
        return;
    }

    display.setRotation(1);
    Serial.println("TFT INIT OK");
    digitSprite.setColorDepth(16);
    if (digitSprite.createSprite(48, 90) == nullptr)
    {
        Serial.println("DIGIT SPRITE ALLOCATION FAILED");
        return;
    }
    display.fillScreen(kBackgroundColor);
    drawCenteredAt("GYMIO", 70, 30, &fonts::Font4, TFT_WHITE);
    drawCenteredAt("WAITING", 408, 30, &fonts::Font2, kMutedColor);

    display.fillRoundRect(16, 58, 448, 158, 18, kCardColor);
    drawCenteredAt("CURRENT TIME", 240, 82, &fonts::Font2, kMutedColor);
    updateTimer(0);

    display.fillRoundRect(16, 224, 216, 80, 18, kCardColor);
    drawCenteredAt("RIDER", 124, 243, &fonts::Font2, kMutedColor);
    drawCenteredAt("NONAME", 124, 279, &fonts::Font4, TFT_WHITE);

    display.fillRoundRect(248, 224, 216, 80, 18, kCardColor);
    drawCenteredAt("BEST", 356, 243, &fonts::Font2, kMutedColor);
    drawCenteredAt("--:--.---", 356, 269, &fonts::Font4, TFT_WHITE);
    displayedBestMs = 0;
    lastTimerUpdateMs = millis();
    Serial.println("Large-timer Gymio layout rendered. Starting ESP-NOW...");

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setSleep(false);
    if (esp_wifi_set_channel(Config::kWifiAccessPointChannel, WIFI_SECOND_CHAN_NONE) == ESP_OK &&
        esp_now_init() == ESP_OK)
    {
        espNowReady = esp_now_register_recv_cb(receiveEspNow) == ESP_OK;
    }
    Serial.println(espNowReady ? "Display ESP-NOW receiver ready."
                               : "Display ESP-NOW receiver initialization failed.");
    if (!espNowReady)
    {
        updateHeaderState("RADIO ERROR");
    }
}

void loop()
{
    const uint32_t now = millis();
    if (espNowReady)
    {
        receiveStatus(now);
        if (lastStatusPacketMs != 0U && now - lastStatusPacketMs > 1500U)
        {
            updateHeaderState("NO DATA");
        }
    }
    delay(10);
}

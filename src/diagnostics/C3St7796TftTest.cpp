#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace
{
constexpr int8_t kTftSckPin = 4;
constexpr int8_t kTftMosiPin = 6;
constexpr int8_t kTftCsPin = 7;
constexpr int8_t kTftDcPin = 3;
constexpr int8_t kTftResetPin = 10;
constexpr uint32_t kSpiWriteFrequencyHz = 20000000;
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
char previousTimerText[10] = "         ";

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
        const int32_t x = startX + static_cast<int32_t>(i) * cellWidth;
        char glyph[2] = {text[i], '\0'};
        digitSprite.fillSprite(kCardColor);
        digitSprite.setFont(&fonts::Font8);
        digitSprite.setTextColor(TFT_WHITE, kCardColor);
        digitSprite.setTextDatum(lgfx::textdatum_t::middle_center);
        digitSprite.drawString(glyph, cellWidth / 2, 45);
        digitSprite.pushSprite(x, startY);
        previousTimerText[i] = text[i];
    }
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32-C3 ST7796S INIT ONLY - NO WIFI");
    Serial.println("Initializing display controller at 20 MHz...");

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
    drawCenteredAt("READY", 414, 30, &fonts::Font2, kAccentColor);

    display.fillRoundRect(16, 58, 448, 158, 18, kCardColor);
    drawCenteredAt("CURRENT TIME", 240, 82, &fonts::Font2, kMutedColor);
    updateTimer(0);

    display.fillRoundRect(16, 228, 216, 76, 18, kCardColor);
    drawCenteredAt("RIDER", 124, 249, &fonts::Font2, kMutedColor);
    drawCenteredAt("NONAME", 124, 282, &fonts::Font4, TFT_WHITE);

    display.fillRoundRect(248, 228, 216, 76, 18, kCardColor);
    drawCenteredAt("BEST", 356, 249, &fonts::Font2, kMutedColor);
    drawCenteredAt("--:--.---", 356, 282, &fonts::Font4, TFT_WHITE);
    lastTimerUpdateMs = millis();
    Serial.println("Large-timer Gymio layout rendered.");
}

void loop()
{
    const uint32_t now = millis();
    if (now - lastTimerUpdateMs >= 1000U)
    {
        lastTimerUpdateMs = now;
        updateTimer((now / 1000U) * 1000U);
    }
    delay(10);
}

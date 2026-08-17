#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "config/Config.h"

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
WiFiUDP statusUdp;
uint32_t lastTimerUpdateMs = 0;
uint32_t lastWifiReconnectMs = 0;
uint32_t lastStatusPacketMs = 0;
char previousTimerText[10] = "         ";
char displayedRider[25] = "NONAME";
char displayedBestRider[25] = "";
char displayedState[16] = "";
uint32_t displayedBestMs = UINT32_MAX;
wl_status_t displayedWifiStatus = WL_NO_SHIELD;
bool wifiHasConnected = false;
bool udpStarted = false;

void handleWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
        Serial.printf("[%lu ms] WiFi disconnected, reason=%u\n",
                      static_cast<unsigned long>(millis()),
                      static_cast<unsigned>(info.wifi_sta_disconnected.reason));
    }
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

void formatTime(uint32_t timeMs, char *output, size_t outputSize)
{
    snprintf(output, outputSize, "%02lu:%02lu.%03lu",
             static_cast<unsigned long>(timeMs / 60000U),
             static_cast<unsigned long>((timeMs / 1000U) % 60U),
             static_cast<unsigned long>(timeMs % 1000U));
}

bool readJsonString(const char *json, const char *key, char *output, size_t outputSize)
{
    char marker[40];
    snprintf(marker, sizeof(marker), "\"%s\":\"", key);
    const char *start = strstr(json, marker);
    if (start == nullptr)
    {
        return false;
    }
    start += strlen(marker);
    const char *end = strchr(start, '"');
    if (end == nullptr)
    {
        return false;
    }
    const size_t length = min(static_cast<size_t>(end - start), outputSize - 1U);
    memcpy(output, start, length);
    output[length] = '\0';
    return true;
}

bool readJsonUint(const char *json, const char *key, uint32_t &value)
{
    char marker[40];
    snprintf(marker, sizeof(marker), "\"%s\":", key);
    const char *start = strstr(json, marker);
    if (start == nullptr)
    {
        return false;
    }
    value = strtoul(start + strlen(marker), nullptr, 10);
    return true;
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
    int packetSize = statusUdp.parsePacket();
    while (packetSize > 0)
    {
        char payload[256];
        const int length = statusUdp.read(payload, sizeof(payload) - 1U);
        if (length > 0)
        {
            payload[length] = '\0';
            char state[16];
            char rider[25];
            char bestRider[25];
            uint32_t timeMs = 0;
            uint32_t bestMs = 0;
            if (readJsonString(payload, "state", state, sizeof(state)) &&
                readJsonString(payload, "riderName", rider, sizeof(rider)) &&
                readJsonString(payload, "absoluteBestRider", bestRider, sizeof(bestRider)) &&
                readJsonUint(payload, "timeMs", timeMs) &&
                readJsonUint(payload, "absoluteBestMs", bestMs))
            {
                lastStatusPacketMs = nowMs;
                updateHeaderState(state);
                updateTimer(timeMs);
                updateRider(rider);
                updateBest(bestMs, bestRider);
            }
        }
        packetSize = statusUdp.parsePacket();
    }
}

void drawWifiStatus(wl_status_t status)
{
    if (status == displayedWifiStatus)
    {
        return;
    }
    displayedWifiStatus = status;
    displayedState[0] = '\0';
    Serial.printf("WiFi status: %d\n", static_cast<int>(status));
    if (status == WL_CONNECTED)
    {
        wifiHasConnected = true;
        WiFi.setSleep(true);
        Serial.printf("[%lu ms] WiFi connected, RSSI %ld dBm, IP: ",
                      static_cast<unsigned long>(millis()),
                      static_cast<long>(WiFi.RSSI()));
        Serial.println(WiFi.localIP());
    }
    const char *label = "OFFLINE";
    uint16_t color = kMutedColor;
    switch (status)
    {
    case WL_IDLE_STATUS:
        label = "CONNECTING";
        break;
    case WL_NO_SSID_AVAIL:
        label = "NO AP";
        break;
    case WL_CONNECTED:
        label = "ONLINE";
        color = kAccentColor;
        break;
    case WL_CONNECT_FAILED:
        // A C3 may briefly report this while the AP handshake is still being
        // retried. The credentials are known-good once this unit has connected.
        label = "CONNECTING";
        break;
    case WL_CONNECTION_LOST:
        label = "LOST";
        break;
    case WL_DISCONNECTED:
        label = wifiHasConnected ? "OFFLINE" : "CONNECTING";
        break;
    default:
        label = "WAITING";
        break;
    }
    display.fillRect(340, 5, 135, 48, kBackgroundColor);
    drawCenteredAt(label, 408, 30, &fonts::Font2, color);
}

void updateWifi(uint32_t nowMs)
{
    const wl_status_t status = WiFi.status();
    drawWifiStatus(status);
    if (status == WL_CONNECTED && !udpStarted)
    {
        udpStarted = statusUdp.begin(Config::kDisplayStatusPort);
        Serial.println(udpStarted ? "Display UDP receiver ready." : "Display UDP receiver failed.");
    }
    if (status != WL_CONNECTED && udpStarted)
    {
        statusUdp.stop();
        udpStarted = false;
    }
    if (status != WL_CONNECTED && nowMs - lastWifiReconnectMs >= 30000U)
    {
        Serial.println("WiFi recovery reconnect requested.");
        WiFi.reconnect();
        lastWifiReconnectMs = nowMs;
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
    Serial.println("Large-timer Gymio layout rendered. Starting WiFi...");

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(handleWifiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.config(IPAddress(192, 168, 4, 250),
                IPAddress(192, 168, 4, 1),
                IPAddress(255, 255, 255, 0));
    WiFi.begin(Config::kWifiAccessPointName, Config::kWifiAccessPointPassword);
    lastWifiReconnectMs = millis();
}

void loop()
{
    const uint32_t now = millis();
    updateWifi(now);
    if (udpStarted)
    {
        receiveStatus(now);
        if (lastStatusPacketMs != 0U && now - lastStatusPacketMs > 1500U)
        {
            updateHeaderState("NO DATA");
        }
    }
    delay(10);
}

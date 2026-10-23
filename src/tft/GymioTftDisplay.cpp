#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "config/Config.h"
#include "config/Version.h"
#include "ota/OtaUpdateServer.h"
#include "protocol/DisplayStatusProtocol.h"
#include "protocol/DeviceStatusProtocol.h"
#include "protocol/GateControlProtocol.h"
#include "protocol/OtaControlProtocol.h"
#include "tft/TftHardwareConfig.h"

namespace
{
// Sunlight-oriented palette: maximize luminance separation on the TFT.
constexpr uint16_t kBackgroundColor = TFT_BLACK;
constexpr uint16_t kCardColor = 0x1082;
constexpr uint16_t kMutedColor = 0xCE59;
constexpr uint16_t kAccentColor = 0xDFA0;
constexpr uint8_t kCancelButtonPin = 20;
constexpr uint32_t kButtonDebounceMs = 40;
constexpr uint32_t kCommandRepeatIntervalMs = 40;
constexpr uint8_t kCommandRepeatCount = 5;
constexpr uint32_t kTimerRenderIntervalMs = 25;
constexpr uint8_t kBroadcastAddress[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

class St7796Display final : public lgfx::LGFX_Device
{
public:
    St7796Display()
    {
        auto busConfig = bus_.config();
        busConfig.spi_host = SPI2_HOST;
        busConfig.spi_mode = 0;
        busConfig.freq_write = TftHardwareConfig::kSpiWriteFrequencyHz;
        busConfig.freq_read = 10000000;
        busConfig.spi_3wire = false;
        busConfig.use_lock = true;
        busConfig.dma_channel = SPI_DMA_CH_AUTO;
        busConfig.pin_sclk = TftHardwareConfig::kSclkPin;
        busConfig.pin_mosi = TftHardwareConfig::kMosiPin;
        busConfig.pin_miso = TftHardwareConfig::kMisoPin;
        busConfig.pin_dc = TftHardwareConfig::kDcPin;
        bus_.config(busConfig);
        panel_.setBus(&bus_);

        auto panelConfig = panel_.config();
        panelConfig.pin_cs = TftHardwareConfig::kCsPin;
        panelConfig.pin_rst = TftHardwareConfig::kResetPin;
        panelConfig.pin_busy = -1;
        panelConfig.panel_width = TftHardwareConfig::kPanelWidth;
        panelConfig.panel_height = TftHardwareConfig::kPanelHeight;
        panelConfig.memory_width = TftHardwareConfig::kPanelWidth;
        panelConfig.memory_height = TftHardwareConfig::kPanelHeight;
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
OtaUpdateServer otaServer("Gymio TFT display", IPAddress(192, 168, 4, 3));
uint32_t lastTimerUpdateMs = 0;
uint32_t lastStatusPacketMs = 0;
uint32_t lastDeviceStatusMs = 0;
uint32_t timerAnchorMs = 0;
uint32_t timerAnchorReceivedAtMs = 0;
bool timerRunning = false;
char previousTimerText[10] = "         ";
char displayedRider[25] = "NONAME";
char displayedBestRider[25] = "";
char displayedState[16] = "";
uint8_t displayedGateCount = 0;
uint32_t displayedBestMs = UINT32_MAX;
bool readyMessageDisplayed = false;
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
DisplayStatusProtocol::Frame pendingFrame;
volatile bool framePending = false;
bool espNowReady = false;
bool cancelButtonRaw = HIGH;
bool cancelButtonStable = HIGH;
uint32_t cancelButtonChangedAtMs = 0;
uint32_t controlBootId = 0;
uint32_t controlSequence = 0;
uint32_t nextCommandSendMs = 0;
uint8_t commandRepeatsRemaining = 0;
volatile bool otaRequested = false;
uint8_t radioChannel = Config::kWifiAccessPointChannel;
uint32_t lastChannelHopMs = 0;
constexpr uint32_t kChannelHopIntervalMs = 300;

void setBacklightBrightness(uint8_t percent)
{
    percent = min(percent, static_cast<uint8_t>(100));
    constexpr uint16_t kMaximumDuty =
        (1U << TftHardwareConfig::kBacklightPwmResolutionBits) - 1U;
    const uint32_t duty = (static_cast<uint32_t>(percent) * kMaximumDuty + 50U) / 100U;
    ledcWrite(TftHardwareConfig::kBacklightPwmChannel, duty);
}

void setBacklightEnabled(bool enabled)
{
    setBacklightBrightness(enabled ? TftHardwareConfig::kDefaultBacklightPercent : 0U);
}

void receiveEspNow(const uint8_t *, const uint8_t *data, int length)
{
    if (length == static_cast<int>(sizeof(OtaControlProtocol::Command)))
    {
        OtaControlProtocol::Command command;
        memcpy(&command, data, sizeof(command));
        if (command.magic == OtaControlProtocol::kMagic &&
            command.version == OtaControlProtocol::kVersion &&
            command.target == OtaControlProtocol::Target::TftDisplay)
        {
            otaRequested = true;
        }
        return;
    }
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

void serviceCancelButton(uint32_t nowMs)
{
    const bool raw = digitalRead(kCancelButtonPin);
    if (raw != cancelButtonRaw)
    {
        cancelButtonRaw = raw;
        cancelButtonChangedAtMs = nowMs;
    }
    if (raw != cancelButtonStable && nowMs - cancelButtonChangedAtMs >= kButtonDebounceMs)
    {
        cancelButtonStable = raw;
        if (cancelButtonStable == LOW)
        {
            ++controlSequence;
            commandRepeatsRemaining = kCommandRepeatCount;
            nextCommandSendMs = 0;
            Serial.println("Cancel button pressed.");
        }
    }

    if (!espNowReady || commandRepeatsRemaining == 0 ||
        (nextCommandSendMs != 0 && nowMs < nextCommandSendMs))
    {
        return;
    }

    GateControlProtocol::Command command;
    command.bootId = controlBootId;
    command.sequence = controlSequence;
    const esp_err_t result = esp_now_send(
        kBroadcastAddress, reinterpret_cast<const uint8_t *>(&command), sizeof(command));
    if (result != ESP_OK)
    {
        Serial.printf("Cancel command send failed: %d\n", static_cast<int>(result));
    }
    --commandRepeatsRemaining;
    nextCommandSendMs = nowMs + kCommandRepeatIntervalMs;
}

void sendDeviceStatus(uint32_t nowMs)
{
    if (!espNowReady || nowMs - lastDeviceStatusMs < 1000)
        return;
    lastDeviceStatusMs = nowMs;
    DeviceStatusProtocol::Frame frame;
    frame.device = DeviceStatusProtocol::Device::TftDisplay;
    frame.bootId = controlBootId;
    strlcpy(frame.firmwareVersion, Version::kString, sizeof(frame.firmwareVersion));
    esp_now_send(kBroadcastAddress, reinterpret_cast<const uint8_t *>(&frame), sizeof(frame));
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

void updateTimer(uint32_t elapsedMs, bool showReadyMessage = false)
{
    constexpr int32_t cellWidth = 48;
    constexpr int32_t startX = 24;
    constexpr int32_t startY = 105;

    if (showReadyMessage)
    {
        if (!readyMessageDisplayed)
        {
            display.fillRect(startX, startY, cellWidth * 9, 90, kCardColor);
            display.setFont(&fonts::Font4);
            display.setTextSize(2);
            display.setTextColor(TFT_WHITE);
            display.setTextDatum(lgfx::textdatum_t::middle_center);
            display.drawString("READY TO GO", 240, 150);
            display.setTextSize(1);
            readyMessageDisplayed = true;
        }
        return;
    }

    if (readyMessageDisplayed)
    {
        display.fillRect(startX, startY, cellWidth * 9, 90, kCardColor);
        memset(previousTimerText, ' ', sizeof(previousTimerText) - 1U);
        previousTimerText[sizeof(previousTimerText) - 1U] = '\0';
        readyMessageDisplayed = false;
    }

    const uint32_t minutes = elapsedMs / 60000U;
    const uint32_t seconds = (elapsedMs / 1000U) % 60U;
    const uint32_t milliseconds = elapsedMs % 1000U;
    char text[16];
    snprintf(text, sizeof(text), "%02lu:%02lu.%03lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(milliseconds));

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

void updateGateCount(uint8_t gateCount)
{
    gateCount = gateCount == 2 ? 2 : 1;
    if (gateCount == displayedGateCount)
    {
        return;
    }
    displayedGateCount = gateCount;
    display.fillRect(195, 5, 135, 48, kBackgroundColor);
    drawCenteredAt(gateCount == 2 ? "2 GATES" : "1 GATE",
                   262, 30, &fonts::Font2, kAccentColor);
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
    timerAnchorMs = frame.timeMs;
    timerAnchorReceivedAtMs = nowMs;
    timerRunning = strcmp(frame.state, "RUNNING") == 0 ||
                   strcmp(frame.state, "CLEAR GATE") == 0;
    updateGateCount(frame.gateCount);
    updateHeaderState(frame.state);
    updateTimer(frame.timeMs, strcmp(frame.state, "READY") == 0);
    updateRider(frame.riderName);
    updateBest(frame.absoluteBestMs, frame.absoluteBestRider);
}
} // namespace

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("GYMIO ESP32-C3 TFT DISPLAY");
    Serial.printf("Hardware profile: %s\n", TftHardwareConfig::kProfileName);
    pinMode(kCancelButtonPin, INPUT_PULLUP);
    cancelButtonRaw = digitalRead(kCancelButtonPin);
    cancelButtonStable = cancelButtonRaw;
    cancelButtonChangedAtMs = millis();
    controlBootId = esp_random();
    ledcSetup(TftHardwareConfig::kBacklightPwmChannel,
              TftHardwareConfig::kBacklightPwmFrequencyHz,
              TftHardwareConfig::kBacklightPwmResolutionBits);
    ledcAttachPin(TftHardwareConfig::kBacklightPin,
                  TftHardwareConfig::kBacklightPwmChannel);
    setBacklightEnabled(true);
    Serial.printf("TFT backlight: %u%%\n", TftHardwareConfig::kDefaultBacklightPercent);
    Serial.printf("Initializing %s controller at %lu MHz...\n",
                  TftHardwareConfig::kController,
                  static_cast<unsigned long>(TftHardwareConfig::kSpiWriteFrequencyHz / 1000000U));

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
    drawCenteredAt("1 GATE", 262, 30, &fonts::Font2, kMutedColor);
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
        esp_now_peer_info_t gatePeer{};
        memcpy(gatePeer.peer_addr, kBroadcastAddress, ESP_NOW_ETH_ALEN);
        gatePeer.channel = 0;
        gatePeer.ifidx = WIFI_IF_STA;
        gatePeer.encrypt = false;
        espNowReady = esp_now_add_peer(&gatePeer) == ESP_OK &&
                      esp_now_register_recv_cb(receiveEspNow) == ESP_OK;
    }
    Serial.println(espNowReady ? "Display ESP-NOW receiver ready."
                               : "Display ESP-NOW receiver initialization failed.");
    if (!espNowReady)
    {
        updateHeaderState("RADIO ERROR");
    }
}

void serviceRadioDiscovery(uint32_t nowMs)
{
    if (otaServer.active() ||
        (lastStatusPacketMs != 0U && nowMs - lastStatusPacketMs <= 1500U) ||
        nowMs - lastChannelHopMs < kChannelHopIntervalMs)
    {
        return;
    }
    radioChannel = radioChannel >= 13 ? 1 : radioChannel + 1;
    esp_wifi_set_channel(radioChannel, WIFI_SECOND_CHAN_NONE);
    lastChannelHopMs = nowMs;
}

void loop()
{
    const uint32_t now = millis();
    if (otaRequested)
    {
        otaRequested = false;
        otaServer.begin(now);
        updateHeaderState("OTA MODE");
    }
    otaServer.service(now);
    if (otaServer.active())
    {
        delay(10);
        return;
    }
    serviceCancelButton(now);
    if (espNowReady)
    {
        sendDeviceStatus(now);
        receiveStatus(now);
        serviceRadioDiscovery(now);
        if (lastStatusPacketMs != 0U && now - lastStatusPacketMs > 1500U)
        {
            timerRunning = false;
            updateHeaderState("NO DATA");
        }
        else if (timerRunning && now - lastTimerUpdateMs >= kTimerRenderIntervalMs)
        {
            // ESP-NOW status frames provide authoritative checkpoints. Advance
            // the visible timer locally between them so the large display is
            // smooth even when a frame is delayed or lost.
            lastTimerUpdateMs = now;
            updateTimer(timerAnchorMs + (now - timerAnchorReceivedAtMs));
        }
    }
    delay(10);
}

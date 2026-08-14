#if defined(GYMIO_POWER_DIAGNOSTIC)

#include <Arduino.h>
#include <WiFi.h>

namespace
{
constexpr char kTestSsid[] = "GymioPowerTest";
constexpr char kTestPassword[] = "gymio123";
uint32_t lastReportMs = 0;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("GYMIO POWER DIAGNOSTIC");
    Serial.println("Starting Wi-Fi access point...");

    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(kTestSsid, kTestPassword))
    {
        Serial.println("Wi-Fi AP start failed");
        return;
    }

    Serial.println("Wi-Fi AP started");
    Serial.print("SSID: ");
    Serial.println(kTestSsid);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
}

void loop()
{
    const uint32_t nowMs = millis();
    if (nowMs - lastReportMs >= 1000)
    {
        lastReportMs = nowMs;
        Serial.print("Stable uptime: ");
        Serial.print(nowMs / 1000);
        Serial.println(" s");
    }
}

#endif

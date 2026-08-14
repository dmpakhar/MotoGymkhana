#if defined(GYMIO_OLED_DIAGNOSTIC)

#include <Arduino.h>

#include "display/Display.h"

namespace
{
Display display;
uint32_t lastUpdateMs = 0;
bool showFirstValue = true;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("GYMIO OLED DIAGNOSTIC");
    if (!display.begin())
    {
        Serial.println("OLED INIT FAILED");
        return;
    }

    Serial.println("OLED OK");
}

void loop()
{
    const uint32_t nowMs = millis();
    if (nowMs - lastUpdateMs < 1000)
    {
        return;
    }

    lastUpdateMs = nowMs;
    const int16_t valueMm = showFirstValue ? 111 : 888;
    showFirstValue = !showFirstValue;
    display.showDistance(valueMm);
    Serial.print("OLED renders: ");
    Serial.println(valueMm);
}

#endif

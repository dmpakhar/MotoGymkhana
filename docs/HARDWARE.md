# Gymio hardware

## Main/start gate — ESP32-S3 N8R8

PlatformIO environment: `esp32-s3-n8r8`.

The OLED and distance sensor use separate I2C controllers.

| Device pin | ESP32-S3 pin | Purpose |
| --- | --- | --- |
| OLED VCC | 3.3V | OLED power |
| OLED GND | GND | Ground |
| OLED SDA | GPIO10 | OLED I2C data |
| OLED SCL | GPIO11 | OLED I2C clock |
| VL53L1X VIN | 3.3V | Sensor power |
| VL53L1X GND | GND | Ground |
| VL53L1X SDA | GPIO6 | Sensor I2C data |
| VL53L1X SCL | GPIO7 | Sensor I2C clock |
| VL53L1X GPIO1 | NC | Not used |
| VL53L1X XSHUT | NC | Not used |

OLED address is `0x3C`; VL53L1X address is `0x29`. The OLED shows the rider,
timer state, distance/result, and `1GM` or `2GM` for the active gate mode.

## Finish gate — ESP32-S3 Mini N4R2

PlatformIO environment: `finish-gate-s3-mini`. The board has 4 MB flash and
2 MB PSRAM.

| Device pin | ESP32-S3 Mini pin | Purpose |
| --- | --- | --- |
| OLED VCC | 3.3V | OLED power |
| OLED GND | GND | Ground |
| OLED SDA | GPIO10 | OLED I2C data |
| OLED SCL | GPIO11 | OLED I2C clock |
| VL53L1X VIN | 3.3V | Sensor power |
| VL53L1X GND | GND | Ground |
| VL53L1X SDA | GPIO6 | Sensor I2C data |
| VL53L1X SCL | GPIO7 | Sensor I2C clock |
| Battery divider output | GPIO1 | Battery ADC |

Battery voltage is measured through a 100 kΩ/100 kΩ divider: battery positive
to 100 kΩ, divider midpoint to GPIO1, and another 100 kΩ from GPIO1 to ground.
All grounds must be common. The firmware treats 3.4 V as empty/non-operational
and 4.2 V as full for the four-section battery indicator. The voltage reading
is diagnostic, not a precision fuel gauge.

## Large display — ESP32-C3-MINI-1 and ST7796S

PlatformIO environment: `tft-c3-mini-1`. The generic TFT is 3.5 inches,
320x480, write-only 4-wire SPI. Configure its interface jumpers as `IM0=1`,
`IM1=1`, `IM2=1`.

| TFT pin | ESP32-C3 pin | Purpose |
| --- | --- | --- |
| VCC | VIN / USB 5V | TFT power; never use ESP32 3.3V |
| GND | GND | Common ground |
| SCL | GPIO5 | SPI SCK, not I2C SCL |
| SDA | GPIO6 | SPI MOSI, not I2C SDA |
| CS | GPIO2 | Chip select |
| DC | GPIO4 | Data/command |
| RST | GPIO8 | Display reset |
| BL | GPIO0 | Active-high 3.3V backlight control/PWM |
| SDA-OF | NC | No MISO/readback |

Power topology:

```text
USB-C 5V ----+----> ESP32-C3
             +----> VIN/VBUS ----> TFT VCC
Common GND -----------------------> TFT GND
```

The TFT PCB contains its own regulator. SPI/control remain 3.3V logic. GPIO0
only controls the onboard backlight circuit and must never supply LED current.
Firmware uses active-high 20 kHz, 8-bit PWM and defaults to 100% brightness.
GPIO9 must not be used for BL on this tested board because it can interfere
with boot. SPI write frequency is kept at the proven 20 MHz.

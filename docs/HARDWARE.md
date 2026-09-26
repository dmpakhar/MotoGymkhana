# Gymio hardware

## Hardware variants

The start and finish gates can use either power configuration:

- **Classic** — no battery or additional power circuitry; powered from USB or
  a suitable regulated board input.
- **Battery Extended** — self-contained 1S 18650 supply with USB-C charging,
  protection, boost/power control, and Schottky isolation.

The power configuration does not change the firmware target or signal pinout.
See [Power configurations](POWER.md) for verified wiring, limitations, and the
planned (currently unassigned) battery ADC circuit.

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

### Compact main gate — ESP32-S3 Mini N4R2

The `main-gate-s3-mini` profile uses the same OLED and VL53L1X pins plus:

| Component | ESP32-S3 Mini pin | Wiring / meaning |
| --- | --- | --- |
| Gate button | GPIO8 | Normally-open button to GND; internal pull-up |
| Blue LED | GPIO9 | Attempt running |
| Yellow LED | GPIO12 | Gate crossing, 600 ms pulse |
| Green LED | GPIO13 | Timer ready |

Connect each LED anode through its own 330–680 Ω resistor to the GPIO and its
cathode to GND. Outputs are active-high. Never connect the button or GPIOs to
5 V.

A short press outside setup cancels an active attempt without saving it. Holding
the button for 1.5 seconds while READY enters gate setup mode. The OLED then
shows the live ToF distance; place the setup target at the required distance and
short-press the button to save it. The OLED confirms `GATE SET` and the distance
for 3 seconds. Attempts remain blocked throughout setup and until the saved
setup target is removed from the beam. The saved threshold uses the rolling
average reference minus a 150 mm safety margin; an `INF` reference becomes
3850 mm. This dead band prevents static ToF jitter from causing crossings.

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
| Gate button | GPIO8 | Normally-open button to GND; internal pull-up |
| Blue LED | GPIO9 | Attempt running |
| Yellow LED | GPIO12 | Finish crossing, 600 ms pulse |
| Green LED | GPIO13 | Connected, calibrated, and timer ready |

Wire the button and LEDs exactly as on the compact main gate: button to GND,
and each active-high LED through its own 330–680 Ω resistor to GND. A short
press sends the same attempt-cancel command as the start gate. Hold for 1.5
seconds while the connected main gate is READY to enter finish-gate setup, then
short-press to save the filtered reference minus the 150 mm safety margin.
Crossings remain blocked until the setup target is removed.

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

Connect the optional attempt-cancel button between `GPIO20` and `GND`. Firmware
uses `INPUT_PULLUP`, so no external pull-up is required. The button is
active-low; never connect GPIO20 to 5 V or directly to another GPIO.

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

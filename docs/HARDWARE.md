# Gymio hardware pinouts

## ESP32-C3 Super Mini ST7796S display (existing profile)

This wiring is used only by the standalone `tft-hardware-test` environment.
The generic 3.5-inch 320x480 board must have `IM0=1`, `IM1=1`, and `IM2=1` for
4-wire SPI mode.

| TFT pin | ESP32-C3 Super Mini | Purpose |
| --- | --- | --- |
| VCC | 5V/VBUS | Display module power; do not connect to 3.3V. |
| GND | GND | Common ground. |
| SCL | GPIO4 | SPI SCK. |
| SDA | GPIO6 | SPI MOSI. |
| CS | GPIO7 | Chip select. |
| DC | GPIO3 | Data/command. |
| RST | GPIO10 | Display reset. |
| BL | GPIO5 | Active-high 3.3 V PWM control; never used as a power source. |
| SDA-OF | Not connected | Not used. |

GPIO logic remains 3.3V. MISO/readback and touch are disabled. GPIO2, GPIO8,
and GPIO9 are not used by this test. TFT `VCC` remains connected to 5V/VBUS;
GPIO5 does not power the backlight.

### Backlight control

The digital LOW/HIGH hardware test confirmed that this module accepts GPIO5 as
an active-high 3.3 V backlight-control input. Firmware uses an ESP32-C3 LEDC
channel at 20 kHz with 8-bit resolution. `setBacklightBrightness(percent)`
accepts values from 0 (off) through 100 (maximum) and clamps larger values.
Default brightness is configured by `kDefaultBacklightPercent` in
`src/tft/GymioTftDisplay.cpp`.

GPIO5 is a control signal only and does not power the backlight. Keep TFT `VCC`
on 5V/VBUS, do not connect `BL` to 5 V, do not use open-drain mode, and retain a
common ground between the TFT and ESP32-C3.

## ESP32-C3-MINI-1 ST7796S display (new profile)

This profile is for the standalone Gymio TFT display built with an
ESP32-C3-MINI-1 module on a small USB-C development board. Select PlatformIO
environment `tft-c3-mini-1`. The generic 3.5-inch TFT uses an ST7796S
controller, 320x480 resolution, and 4-wire SPI with `IM0=1`, `IM1=1`, and
`IM2=1`.

| TFT pin | ESP32-C3-MINI-1 pin | Purpose |
| --- | --- | --- |
| VCC | VIN / USB 5V | TFT module power; never use the 3.3 V output. |
| GND | GND | Common power and signal ground. |
| SCL | GPIO5 | SPI SCK, not I2C SCL. |
| SDA | GPIO6 | SPI MOSI, not I2C SDA. |
| CS | GPIO2 | SPI chip select. |
| DC | GPIO4 | Data/command. |
| RST | GPIO8 | Display reset. |
| BL | GPIO0 | Active-high 3.3 V backlight control/PWM signal. |
| SDA-OF | Not connected | Unused; no MISO/readback is required. |

### Power topology

```text
USB-C 5V
   |
   +--> ESP32-C3 board
   |
   +--> VIN/VBUS --> TFT VCC

ESP32 GND --------> TFT GND
```

The TFT PCB includes an onboard 3.3 V regulator marked `6206A ... /33` and has
been tested successfully from VIN/USB VBUS at approximately 5 V. Do not power
TFT `VCC` from the ESP32-C3's 3.3 V output. SPI and control signals remain
3.3 V logic, and no TFT MISO connection is used.

### Backlight control

On this tested module, `BL` is a logic-level input to the TFT PCB's backlight
circuit. GPIO0 is only a control signal; it does not source the LED backlight
power. Firmware supports active-high on/off control and 20 kHz, 8-bit LEDC PWM
through `setBacklightBrightness(percent)`. The default is 100%.

GPIO0 was selected after hardware testing showed that connecting this TFT's BL
input to GPIO9 interfered with the ESP32-C3 boot strap and could leave the chip
in download mode. Do not reconnect BL to GPIO9.

The profile uses a conservative 20 MHz SPI write clock. The existing Super Mini
profile and all gate/OLED/ToF pin assignments remain unchanged.

## Legacy ESP32-C3 gate wiring

The shared configuration still contains the original ESP32-C3 OLED/ToF pin
mapping for diagnostics, but `platformio.ini` does not currently define a
production ESP32-C3 gate environment. This mapping is not used by the TFT
application and cannot coexist with its wiring: both assign GPIO4/GPIO5 to
different functions.

- Board target: ESP32-C3 DevKitM-1 compatible
- Flash / PSRAM: depends on the specific C3 board; PSRAM is not required.

### Shared I2C bus

The SSD1306 OLED and VL53L1X share one I2C bus. Their addresses are different,
so no I2C conflict occurs.

| Device pin | ESP32-C3 pin |
| --- | --- |
| OLED VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO4 |
| OLED SCL | GPIO5 |

I2C address: `0x3C`.

### VL53L1X timing sensor

The sensor uses the shared I2C bus.

| Device pin | ESP32-C3 pin |
| --- | --- |
| VL53L1X VIN | 3.3V |
| VL53L1X GND | GND |
| VL53L1X SDA | GPIO4 |
| VL53L1X SCL | GPIO5 |
| VL53L1X GPIO1 | Not connected |
| VL53L1X XSHUT | Not connected |

I2C address: `0x29`.

## ESP32-S3 DevKitC-1 N16R8 / N8R8

- N16R8 PlatformIO environment: `esp32-s3`
- N8R8 PlatformIO environment: `esp32-s3-n8r8`

Both variants use the same GPIO wiring. Their configurations differ in flash
size and partition table; both have 8 MB octal PSRAM.

The S3 prototype uses two independent I2C buses. The VL53L1X bus has been moved
away from GPIO4/GPIO5 to help diagnose whether those original pins were damaged.

### OLED I2C bus

| Device pin | ESP32-S3 pin |
| --- | --- |
| OLED VCC | 3.3V |
| OLED GND | GND |
| OLED SDA | GPIO10 |
| OLED SCL | GPIO11 |

I2C address: `0x3C`.

### VL53L1X I2C bus

| Device pin | ESP32-S3 pin |
| --- | --- |
| VL53L1X VIN | 3.3V |
| VL53L1X GND | GND |
| VL53L1X SDA | GPIO6 |
| VL53L1X SCL | GPIO7 |
| VL53L1X GPIO1 | Not connected |
| VL53L1X XSHUT | Not connected |

I2C address: `0x29`.

# Gymio hardware pinouts

## ESP32-C3

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

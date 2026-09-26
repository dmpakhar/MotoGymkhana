# Build and flash Gymio

## Supported targets

| Device | PlatformIO environment | Hardware |
| --- | --- | --- |
| Main/start gate | `esp32-s3-n8r8` | ESP32-S3 N8R8, SSD1306, VL53L1X |
| Compact main/start gate | `main-gate-s3-mini` | ESP32-S3 Mini N4R2, SSD1306, VL53L1X |
| Finish gate | `finish-gate-s3-mini` | ESP32-S3 Mini N4R2, SSD1306, VL53L1X |
| Large display | `tft-c3-mini-1` | ESP32-C3-MINI-1, ST7796S 320x480 |

Run commands from the repository root. PlatformIO downloads the Arduino
framework and declared libraries on the first build.

## Identify ports

```sh
pio device list
```

When more than one board is connected, always select the port explicitly. On
macOS it normally looks like `/dev/cu.usbmodem...`; Linux commonly uses
`/dev/ttyACM...`, and Windows uses `COM...`.

## Main gate

```sh
pio run -e esp32-s3-n8r8
pio run -e esp32-s3-n8r8 -t upload --upload-port MAIN_PORT

# ESP32-S3 Mini N4R2 alternative
pio run -e main-gate-s3-mini
pio run -e main-gate-s3-mini -t upload --upload-port MAIN_PORT
```

The main gate owns timing, history, riders, Wi-Fi, the web API, and selection
between one-gate and two-gate operation.

## Finish gate

```sh
pio run -e finish-gate-s3-mini
pio run -e finish-gate-s3-mini -t upload --upload-port FINISH_PORT
```

The finish gate sends heartbeat, sensor state, crossing counter, and calibration
data to the main gate over ESP-NOW.

## Large TFT

```sh
pio run -e tft-c3-mini-1
pio run -e tft-c3-mini-1 -t upload --upload-port TFT_PORT
```

The TFT is display-only. It receives timer/rider/best data over ESP-NOW and
does not join the `Gymio` Wi-Fi access point.

## Build everything

Run this after shared protocol or configuration changes:

```sh
pio run -e esp32-s3-n8r8 -e main-gate-s3-mini -e finish-gate-s3-mini -e tft-c3-mini-1
```

All four environments must report `SUCCESS`. When an ESP-NOW protocol or
radio channel changes, rebuild and flash every affected endpoint.

## Serial monitor

All targets use 115200 baud:

```sh
pio device monitor --port DEVICE_PORT --baud 115200
```

Close the monitor before uploading if it owns the port. Generated binaries are
stored under `.pio/build/<environment>/`.

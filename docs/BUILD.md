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

## Updating over Wi-Fi (OTA)

OTA becomes available only after this OTA-capable firmware has been installed
on each device once by USB. Build the desired environment normally. Its image
is stored at `.pio/build/<environment>/firmware.bin`.

1. Power the complete Gymio system and connect the operator device to `Gymio`.
2. Ensure the timer shows `READY` and close both sensor-setup modes.
3. Open `http://gymio.local/update` (or `http://192.168.4.1/update`).
4. If administrator authentication was enabled in Configuration, sign in with
   the credentials from `Config.h` (`admin` / `gymio-update` by default).
5. For the main gate, select its own `firmware.bin` and upload it directly.
6. For the finish gate or TFT, press **Prepare**. Wait about three seconds,
   open the displayed device updater, and upload that device's image.

| Device | OTA address | Required build environment |
|---|---|---|
| Main S3 Mini | `192.168.4.1/update` | `main-gate-s3-mini` |
| Main N8R8 | `192.168.4.1/update` | `esp32-s3-n8r8` |
| Finish gate | `192.168.4.2/update` | `finish-gate-s3-mini` |
| TFT display | `192.168.4.3/update` | `tft-c3-mini-1` |

Each image embeds its build-target identity. An updater rejects an image built
for a different target and leaves the running firmware unchanged. The finish
gate and TFT leave maintenance mode automatically after three minutes if no
upload is active. Do not remove power while an upload is in progress.

## CI/CD releases

GitHub Actions builds the three deployed firmware targets on pushes and pull
requests. Pushing a version tag such as `v0.4.0` creates a GitHub Release containing:

- `gymio-main-gate-s3-mini.bin`
- `gymio-finish-gate-s3-mini.bin`
- `gymio-tft-c3-mini-1.bin`

The target name is deliberately part of each filename and is also verified by
the device during OTA.

## Serial monitor

All targets use 115200 baud:

```sh
pio device monitor --port DEVICE_PORT --baud 115200
```

Close the monitor before uploading if it owns the port. Generated binaries are
stored under `.pio/build/<environment>/`.

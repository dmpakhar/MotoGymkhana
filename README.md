# Gymio

Gymio is a standalone motorcycle gymkhana single-gate timer built on ESP32. An
ESP32-S3 gate controller measures crossings with a VL53L1X sensor, drives a
local OLED, and hosts operator and rider pages over its own Wi-Fi network. An
optional ESP32-C3/ST7796S large display receives live results over ESP-NOW.

Current prerelease: **0.2.0**

## Features

- Automatic start and finish detection through one physical gate.
- Filtered distance measurements and configurable crossing threshold.
- OLED status, rider name, running time, and result display.
- Local Wi-Fi access point; no internet connection is required.
- Operator dashboard for riders, attempts, validity, and sensor setup.
- Per-phone rider registration and explicit preparation for the next ride.
- Personal best and five latest valid times on the rider page.
- Absolute session best, attempt reassignment, invalidation, and revalidation.
- ESP-NOW gate-to-TFT link that does not consume a phone Wi-Fi client slot.
- Sunlight-oriented 480x320 TFT UI with GPIO5 PWM backlight control.
- ESP32-S3 N16R8/N8R8 gate targets and an ESP32-C3 TFT target.

## Quick start

1. Wire the gate OLED and VL53L1X as described in [Hardware](docs/HARDWARE.md).
2. Install [PlatformIO](https://platformio.org/).
3. Build the environment matching the board:

   ```sh
   pio run -e esp32-s3-n8r8
   ```

4. Connect the board and upload:

   ```sh
   pio run -e esp32-s3-n8r8 -t upload
   ```

5. Join Wi-Fi network `Gymio` using password `gymio123`.
6. Open the appropriate page:

   - Main dashboard: `http://gymio.local/main`
   - Rider page: `http://gymio.local/rider`
   - IP fallback: replace `gymio.local` with `192.168.4.1`

The default Wi-Fi password is intended for development. Change it in
`src/config/Config.h` before public use.

To build and upload the optional TFT display:

```sh
pio run -e tft-hardware-test -t upload
```

The environment name is retained for compatibility, but it builds the current
live TFT application. Both devices must use the same channel configured by
`kWifiAccessPointChannel`.

## Documentation

- [Hardware and wiring](docs/HARDWARE.md)
- [User guide and web pages](docs/USER_GUIDE.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Development, diagnostics, and releases](docs/DEVELOPMENT.md)
- [TFT display and ESP-NOW](docs/TFT_DISPLAY.md)
- [HTTP API](docs/API.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Changelog](CHANGELOG.md)

## Prerelease limitations

- Riders and attempt history are held in RAM and are lost when the ESP32 restarts.
- The attempt-history ring buffer retains the latest 20 attempts.
- Up to 20 named riders can be registered, while the SoftAP itself is configured
  for at most 15 simultaneous Wi-Fi clients.
- Phone identity uses browser local storage. Private browsing or cleared site data
  creates a new identity.
- The web interface has no operator authentication.
- ESP-NOW display frames are broadcast and unencrypted.

## License

No license has been declared yet. All rights remain with the repository owner.

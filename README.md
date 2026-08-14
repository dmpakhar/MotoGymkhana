# Gymio

Gymio is a standalone motorcycle gymkhana single-gate timer built on ESP32.
It measures gate crossings with a VL53L1X time-of-flight sensor, displays timer
state on an SSD1306 OLED, and hosts operator and rider web pages over its own
Wi-Fi access point.

Current prerelease: **0.1.0**

## Features

- Automatic start and finish detection through one physical gate.
- Filtered distance measurements and configurable crossing threshold.
- OLED status, rider name, running time, and result display.
- Local Wi-Fi access point; no internet connection is required.
- Operator dashboard for riders, attempts, validity, and sensor setup.
- Per-phone rider registration and explicit preparation for the next ride.
- Personal best and five latest valid times on the rider page.
- ESP32-S3 N16R8, ESP32-S3 N8R8, and ESP32-C3 build targets.

## Quick start

1. Wire the OLED and VL53L1X as described in [Hardware](docs/HARDWARE.md).
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

## Documentation

- [Hardware and wiring](docs/HARDWARE.md)
- [User guide and web pages](docs/USER_GUIDE.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Development, diagnostics, and releases](docs/DEVELOPMENT.md)
- [Changelog](CHANGELOG.md)

## Prerelease limitations

- Riders and attempt history are held in RAM and are lost when the ESP32 restarts.
- The attempt-history ring buffer retains the latest 20 attempts.
- Phone identity uses browser local storage. Private browsing or cleared site data
  creates a new identity.
- The web interface has no operator authentication.

## License

No license has been declared yet. All rights remain with the repository owner.

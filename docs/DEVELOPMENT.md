# Development and releases

For complete device-specific build, upload, port-selection, and monitoring
instructions, see [Build and flash Gymio](BUILD.md).

## PlatformIO environments

| Environment | Hardware |
| --- | --- |
| `esp32-s3` | ESP32-S3 DevKitC-1 N16R8, 16 MB flash / 8 MB octal PSRAM. |
| `esp32-s3-n8r8` | ESP32-S3 DevKitC-1 N8R8, 8 MB flash / 8 MB octal PSRAM. |
| `tft-hardware-test` | ESP32-C3 Super Mini + ST7796S live display application (legacy environment name). |
| `tft-c3-mini-1` | ESP32-C3-MINI-1 + ST7796S live display application. |

Build all targets individually:

```sh
pio run -e esp32-s3
pio run -e esp32-s3-n8r8
pio run -e tft-hardware-test
pio run -e tft-c3-mini-1
```

Upload a selected target:

```sh
pio run -e esp32-s3-n8r8 -t upload
```

Monitor serial output at 115200 baud:

```sh
pio device monitor -b 115200
```

## Diagnostics

Diagnostic source files live in `src/diagnostics` and are guarded by build-time
macros so they do not replace the production `setup()` and `loop()` accidentally.

### ESP32-C3 ST7796S display application

The TFT application lives in `src/tft`. The `tft-hardware-test` environment is
intentionally isolated with `build_src_filter`; it compiles only
`tft/GymioTftDisplay.cpp`. Build, upload, and monitor it with:

```sh
pio run -e tft-hardware-test
pio run -e tft-hardware-test -t upload
pio device monitor -b 115200
```

The application uses LovyanGFX, write-only SPI at 40 MHz, and a 480x320
landscape view. It receives live timer status from the start gate over
broadcast ESP-NOW on the configured access-point channel; it does not join the
phone Wi-Fi network. It shows the active rider, current/result time, absolute
best time and best rider, or a waiting screen while the start gate is
unavailable. No sensor, local timer, distance, touch, or gate application code
is included.

The TFT uses the profile-selected BL pin with LEDC PWM at 20 kHz/8-bit
resolution. The default is 100%; change `kDefaultBacklightPercent` in
`TftHardwareConfig.h` for a different fixed startup brightness.

## Source layout

| Path | Contents |
| --- | --- |
| `src/main.cpp` | Gate application entry point. |
| `src/config` | Shared pins, timing, radio settings, and firmware version. |
| `src/tof`, `src/gate`, `src/timer` | Measurement and timer logic. |
| `src/display` | Gate OLED rendering. |
| `src/rider` | Rider registry and browser identity associations. |
| `src/web` | SoftAP, web pages, API, history, and ESP-NOW sender. |
| `src/protocol` | Shared gate-to-TFT frame definition. |
| `src/tft` | Isolated ESP32-C3 large-display application. |
| `src/diagnostics` | Optional hardware diagnostics. |

| Macro | Diagnostic |
| --- | --- |
| `GYMIO_OLED_DIAGNOSTIC` | OLED-only initialization and rendering. |
| `GYMIO_POWER_DIAGNOSTIC` | Wi-Fi AP and stable-uptime power test. |
| `GYMIO_TOF_L0_DIAGNOSTIC` | Basic ToF serial output. |
| `GYMIO_TOF_L1_DIAGNOSTIC` | Direct VL53L1X test. |
| `GYMIO_TOF_OLED_DIAGNOSTIC` | Combined ToF and OLED test. |

Enable only one diagnostic macro at a time and exclude `src/main.cpp` or add a
dedicated PlatformIO diagnostic environment before building it. Diagnostic code
is development tooling and is not active in the normal board environments.

## Versioning

Gymio follows Semantic Versioning. During initial development, `0.y.z` releases
may make incompatible changes. The release version is recorded in:

- `VERSION` for tools and humans;
- `src/config/Version.h` for firmware and API output;
- `CHANGELOG.md` for release notes;
- Git tag `v<version>` for repository releases.

When releasing:

1. Update both version files.
2. Move changes from the changelog's Unreleased section into a versioned section.
3. Build every supported environment.
4. Commit with a release message.
5. Create and push the matching annotated Git tag.

# Development and releases

## PlatformIO environments

| Environment | Hardware |
| --- | --- |
| `esp32-s3` | ESP32-S3 DevKitC-1 N16R8, 16 MB flash / 8 MB octal PSRAM. |
| `esp32-s3-n8r8` | ESP32-S3 DevKitC-1 N8R8, 8 MB flash / 8 MB octal PSRAM. |
| `esp32-c3` | ESP32-C3 DevKitM-1 compatible board. |
| `tft-hardware-test` | Standalone ESP32-C3 Super Mini + ST7796S SPI display test. |

Build all targets individually:

```sh
pio run -e esp32-s3
pio run -e esp32-s3-n8r8
pio run -e esp32-c3
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

### ESP32-C3 ST7796S hardware test

The `tft-hardware-test` environment is intentionally isolated with
`build_src_filter`; it compiles only `C3St7796TftTest.cpp`. Build, upload, and
monitor it with:

```sh
pio run -e tft-hardware-test
pio run -e tft-hardware-test -t upload
pio device monitor -b 115200
```

The test uses LovyanGFX, write-only SPI at 20 MHz, and a 480x320 landscape view.
It joins the start gate's `Gymio` access point and polls `/api/status` at
`192.168.4.1` twice per second. It shows the active rider, current/result time,
and rider best, or a waiting screen while the start gate is unavailable. No
sensor, local timer, distance, framebuffer, touch, or other production Gymio
code is included.

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

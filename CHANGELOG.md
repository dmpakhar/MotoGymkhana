# Changelog

All notable changes are documented here. Versions follow Semantic Versioning.

## Unreleased

### Documentation

- Added separate build and flash workflows for both ESP32-S3 gate variants and
  the ESP32-C3 TFT module.

## 0.2.0 - 2026-08-17

### Added

- ESP32-C3 ST7796S large-display application with high-contrast landscape UI.
- Broadcast ESP-NOW status protocol between the gate and one or more TFT displays.
- Active-high GPIO5 backlight control using 20 kHz, 8-bit LEDC PWM.
- Absolute best time/rider display and completed-attempt reassignment.

### Changed

- TFT rendering uses 40 MHz SPI and redraws only changed timer glyphs.
- TFT no longer joins the phone Wi-Fi network or consumes a SoftAP client slot.
- SoftAP capacity is configured for 15 simultaneous clients; rider storage remains 20.

## 0.1.0 - 2026-08-14

Initial prerelease.

### Added

- Single-gate VL53L1X timing state machine with OLED output.
- Local Wi-Fi main dashboard and personal rider page.
- Browser-UUID rider registration, preparation, deletion, and current-attempt display.
- Per-rider best result and five latest valid times.
- Attempt invalidation and revalidation.
- Sensor setup controls and gate-threshold adjustment.
- ESP32-S3 N16R8, ESP32-S3 N8R8, and ESP32-C3 targets.
- Hardware, user, architecture, development, and release documentation.

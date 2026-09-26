# Changelog

All notable changes are documented here. Versions follow Semantic Versioning.

## Unreleased

## 0.3.0 - 2026-09-26

### Documentation

- Updated build, wiring, UI, API, ESP-NOW, and troubleshooting documentation for
  the four supported build targets.
- Documented the `Classic` and verified 1S `Battery Extended` power variants,
  including TP4056 USB-C CC correction, boost supply, Schottky isolation,
  limitations, and the unassigned planned battery-sense circuit.
- Reworked the user guide into an event-oriented operating manual covering
  riders, operators, gate modes, setup controls, displays, and recovery steps.
- Documented Battery Extended power controls: single-press start, double-press
  shutdown, and the master ground disconnect used for transport and storage.

### Added

- Added the same GPIO8 setup/cancel button and GPIO9/12/13 status LEDs to the
  finish-gate S3 Mini profile, including local normalized setup and wait-clear.
- Compact `main-gate-s3-mini` profile with a GPIO8 attempt-cancel button and
  blue/yellow/green state LEDs on GPIO9/12/13.
- Two-step physical gate setup on the compact main gate: hold to enter setup,
  short-press to save the live distance, then safely wait for the gate to clear.
- Current-attempt cancellation from the main web UI or an optional TFT button
  on GPIO20, transported to the main gate over deduplicated ESP-NOW commands.
- New `tft-c3-mini-1` display profile for ESP32-C3-MINI-1 with ST7796S wiring
  on GPIO5/6/2/4/8/0 and a conservative 20 MHz SPI clock. BL uses GPIO0 to
  avoid the GPIO9 boot strap.
- ESP32-S3 Mini N4R2 finish-gate profile with OLED status, VL53L1X,
  ESP-NOW heartbeat/crossings, and remote sensor setup.
- Automatic one-/two-gate selection with `1GM`/`2GM` OLED indication.
- Persisted minimum valid attempt time and automatic `BELOW MINIMUM` status.

### Changed

- Removed the compact trigger-threshold diagnostic from the lower-right corner
  of the finish-gate OLED for a cleaner user-facing display.
- Physical and web `Use current` setup now derive the threshold from a filtered
  reference with a 150 mm safety margin. An `INF` reference normalizes to
  3850 mm instead of being rejected or saved at the noisy boundary.

- Gate entry now uses raw distance, immediately accepts a valid sample at least
  300 mm inside the threshold, and retains 50 ms confirmation near the edge.
  Release remains filtered with 250 ms confirmation for stability.
- The large TFT shows a high-visibility `READY TO GO` message in READY state.
- Removed battery measurement and indication from the finish-gate firmware,
  ESP-NOW status, web UI, and hardware documentation.
- Removed unused N16R8, C3 Super Mini TFT, and temporary battery diagnostic
  PlatformIO configurations.

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

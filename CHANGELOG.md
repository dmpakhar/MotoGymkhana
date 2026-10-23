# Changelog

All notable changes are documented here. Versions follow Semantic Versioning.

## 0.5.2 - 2026-10-23

### Changed

- One-frame gate crossings now require a deeper threshold margin and a strong
  VL53L1X signal relative to ambient light.
- Weak or marginal readings require normal multi-frame confirmation on the
  main gate, second gate, and single-gate finish path.
- Automatic calibration now uses a trimmed sample set to reject startup
  distance outliers.
- Serial diagnostics now include signal and ambient rates for field tuning.

## 0.5.1 - 2026-10-23

### Fixed

- Device versions now load immediately when the update page opens and refresh
  periodically without requiring a GitHub release check.
- Update controls now remain hidden until a newer compatible release is
  confirmed, including browsers that override the default `hidden` styling.

## 0.5.0 - 2026-10-23

### Added

- Device version reporting from the finish gate and TFT display over ESP-NOW.
- Per-device online, version, and update status in the update center.
- Sequential **Update all devices** workflow, with the main gate updated last.

### Changed

- Redesigned the firmware update page into release, main-gate, second-gate,
  and display panels.
- Device update buttons are shown only when the connected device is older than
  the latest release.

## 0.4.4 - 2026-10-23

### Added

- One-button TFT display update installation from the main-gate update center.

### Changed

- Finish-gate and TFT updates now share one target-aware streaming OTA relay.

## 0.4.3 - 2026-10-23

### Added

- One-button finish-gate update installation from the main-gate update center.
- Streaming GitHub-to-finish OTA relay with firmware target validation.

## 0.4.2 - 2026-10-23

### Changed

- Removed the manual main-gate firmware file picker from the update center.
- Main-gate updates are installed directly from the latest public GitHub
  Release.

## 0.4.1 - 2026-10-23

### Added

- One-button main-gate update installation from the latest public GitHub
  Release, including target validation, inactive OTA partition writes, and
  automatic restart.
- Update-page install control that appears only when a newer release is found.

### Changed

- PlatformIO is pinned to Espressif32 7.0.1 so local and CI firmware builds use
  the same tested Arduino and ESP-NOW APIs.

## 0.4.0 - 2026-10-23

### Changed

- In one-gate mode, the finish cannot arm during the first 1.5 seconds after
  the start. This prevents the gap between a motorcycle's wheels from being
  mistaken for a fully cleared gate and the rear wheel for the finish.
- Main-gate Configuration and OTA authentication can be enabled explicitly;
  new prototype installations start without a login requirement.

### Added

- Main-gate `AP+STA` networking with persistent upstream Wi-Fi settings,
  automatic reconnect, and an unauthenticated-by-default Configuration page.
- GitHub Actions CI/CD for the three deployed firmware targets and automatic
  binary publication when a `v*` release tag is pushed.
- Main-gate update mode with OLED SSID/IP/status indication and a GitHub
  Release check from the firmware update page.
- Automatic ESP-NOW channel discovery on finish-gate and TFT nodes when the
  upstream Wi-Fi moves the shared main-gate radio channel.
- Authenticated browser OTA for both main-gate profiles, the finish gate, and
  the TFT display. Remote endpoints enter a three-minute Wi-Fi maintenance
  mode through ESP-NOW and use reserved addresses.
- Embedded firmware-target markers prevent an OTA image built for one hardware
  profile from being activated on another profile.

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

- Finish crossings are now transmitted immediately over ESP-NOW, with the
  periodic retry heartbeat reduced from 250 to 100 ms.
- Finish-gate physical setup no longer requires a connected main gate; it is
  available as soon as the local ToF sensor is calibrated.
- Removed the compact trigger-threshold diagnostic from the lower-right corner
  of the finish-gate OLED for a cleaner user-facing display.
- Physical and web `Use current` setup now derive the threshold from a filtered
  reference with a 150 mm safety margin. An `INF` reference normalizes to
  3850 mm instead of being rejected or saved at the noisy boundary.

- Gate entry now samples at a 20 ms ToF timing budget, immediately accepts a
  valid sample at least 100 mm inside the threshold, and uses 20 ms confirmation
  near the edge.
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

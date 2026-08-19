# Build and flash Gymio

Gymio has two independent firmware applications:

| Device | PlatformIO environment | Application |
| --- | --- | --- |
| Main gate, ESP32-S3 N8R8 | `esp32-s3-n8r8` | Sensor, timer, OLED, Wi-Fi pages, API, and ESP-NOW transmitter. |
| Main gate, ESP32-S3 N16R8 | `esp32-s3` | Same gate application for the 16 MB flash board. |
| TFT module, ESP32-C3 Super Mini | `tft-hardware-test` | ST7796S UI, backlight PWM, and ESP-NOW receiver. |
| TFT module, ESP32-C3-MINI-1 | `tft-c3-mini-1` | Same TFT application with the new C3-MINI-1 pin profile and 20 MHz SPI. |

The TFT environment retains its original hardware-test name for compatibility,
but it builds the current live display application.

## Requirements

- PlatformIO Core or the PlatformIO IDE extension;
- a data-capable USB cable for each board;
- Python and USB drivers required by PlatformIO for the connected boards;
- this repository checked out locally.

Run all commands from the repository root, where `platformio.ini` is located.
PlatformIO downloads the configured framework and libraries automatically on
the first build.

## Identify serial ports

Connect the board and list detected ports:

```sh
pio device list
```

When multiple boards are connected, always pass the intended port explicitly:

```sh
--upload-port /dev/cu.usbmodemXXX
```

Linux ports commonly look like `/dev/ttyACM0` or `/dev/ttyUSB0`; Windows ports
look like `COM3`. Port names can change after reconnecting a board.

## Build and flash the main gate

First identify whether the ESP32-S3 board is N8R8 or N16R8. Using the wrong
environment may select an incompatible flash size or partition table.

### N8R8 gate

Build:

```sh
pio run -e esp32-s3-n8r8
```

Upload using automatic port selection:

```sh
pio run -e esp32-s3-n8r8 -t upload
```

Upload to an explicit port:

```sh
pio run -e esp32-s3-n8r8 -t upload --upload-port /dev/cu.usbmodemXXX
```

### N16R8 gate

Build and upload:

```sh
pio run -e esp32-s3
pio run -e esp32-s3 -t upload
```

After boot, the gate serial output should report the firmware version, sensor
and OLED initialization, the `Gymio` access point, and:

```text
Display ESP-NOW broadcaster ready.
```

Connect a phone to `Gymio` with the configured password, then verify
`http://192.168.4.1/main` before diagnosing mDNS.

## Build and flash a TFT module

### Existing ESP32-C3 Super Mini profile

Build:

```sh
pio run -e tft-hardware-test
```

Upload using automatic port selection:

```sh
pio run -e tft-hardware-test -t upload
```

Upload to an explicit port:

```sh
pio run -e tft-hardware-test -t upload --upload-port /dev/cu.usbmodemXXX
```

After boot, the TFT serial output should report successful display, sprite,
backlight, and ESP-NOW initialization, including:

```text
Display ESP-NOW receiver ready.
```

The screen initially shows `WAITING`. It changes to the live gate state after
receiving a valid frame. The TFT does not join the `Gymio` Wi-Fi network and
does not require its password or an IP address.

### New ESP32-C3-MINI-1 profile

Build and upload:

```sh
pio run -e tft-c3-mini-1
pio run -e tft-c3-mini-1 -t upload
```

With an explicit port:

```sh
pio run -e tft-c3-mini-1 -t upload --upload-port /dev/cu.usbmodemXXX
```

This selects compile-time flag `GYMIO_TFT_ESP32_C3_MINI_1`, the wiring in
[Hardware](HARDWARE.md), and a 20 MHz SPI write clock. Do not use the existing
`tft-hardware-test` environment for this wiring.

## Flash both devices after protocol changes

The gate and TFT share `src/protocol/DisplayStatusProtocol.h`. If that file or
`kWifiAccessPointChannel` changes, rebuild and flash both devices:

```sh
pio run -e esp32-s3-n8r8 -t upload --upload-port GATE_PORT
pio run -e tft-c3-mini-1 -t upload --upload-port TFT_PORT
```

Replace `GATE_PORT` and `TFT_PORT` with ports returned by `pio device list`.
The order does not matter. During an upgrade, `WAITING` or `NO DATA` is expected
until both endpoints run compatible firmware on the same channel.

## Serial monitoring

All environments use 115200 baud:

```sh
pio device monitor --port /dev/cu.usbmodemXXX --baud 115200
```

Close the monitor before uploading if the platform cannot share the serial
port. Exit PlatformIO Monitor with `Ctrl+C`.

## Build every supported target

Use this before a release or after changing shared source:

```sh
pio run -e tft-hardware-test -e tft-c3-mini-1 -e esp32-s3 -e esp32-s3-n8r8
```

All four environments must report `SUCCESS`.

## Clean rebuild

If incremental output appears stale, clean only the affected environment and
build it again:

```sh
pio run -e tft-hardware-test -t clean
pio run -e tft-hardware-test
```

Substitute the appropriate gate environment when cleaning a gate build.

## Build artifacts

PlatformIO writes generated files under `.pio/build/<environment>/`. Important
artifacts include:

- `firmware.elf` for symbols and debugging;
- `firmware.bin` for the application image;
- `bootloader.bin` and `partitions.bin` used by the upload process.

Normally, use `pio run ... -t upload` rather than manually writing individual
binary files, because PlatformIO applies the correct addresses and board setup.

For wiring, see [Hardware](HARDWARE.md). For runtime connection problems, see
[Troubleshooting](TROUBLESHOOTING.md).

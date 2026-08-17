# TFT display and ESP-NOW

## Purpose

The optional display node is an ESP32-C3 Super Mini connected to a generic
3.5-inch ST7796S panel. It is display-only: timing, riders, history, and sensor
decisions remain on the ESP32-S3 gate controller.

The PlatformIO environment is named `tft-hardware-test` for compatibility with
the original hardware test, but it now builds `src/tft/GymioTftDisplay.cpp`, the
live Gymio display application.

## Display behavior

The landscape 480x320 screen shows:

- timer state;
- current or final time in `MM:SS.mmm` format;
- rider assigned to the current attempt (`NONAME` when unassigned);
- absolute valid best time and its rider.

At boot, `WAITING` is shown until a valid frame arrives. `NO DATA` is shown when
the display previously received data but no frame has arrived for 1.5 seconds.
`RADIO ERROR` means ESP-NOW receiver initialization failed.

Rendering uses write-only SPI at 40 MHz. The timer uses a small sprite per
character and updates only changed characters to minimize visible refresh.

## ESP-NOW link

The gate broadcasts `DisplayStatusProtocol::Frame` every 100 ms. The frame
contains:

| Field | Meaning |
| --- | --- |
| `magic` | Identifies Gymio frames. |
| `version` | Rejects incompatible frame layouts. |
| `sequence` | Monotonically increasing gate transmission counter. |
| `timeMs` | Current elapsed time or held result in milliseconds. |
| `absoluteBestMs` | Fastest valid attempt, or zero if none exists. |
| `state` | `CALIBRATING`, `READY`, `CLEAR GATE`, `RUNNING`, or `RESULT`. |
| `riderName` | Rider locked to the attempt or currently prepared rider. |
| `absoluteBestRider` | Rider owning the absolute valid best. |

The gate sends to the broadcast MAC on its SoftAP interface. TFT nodes listen
in station mode without joining the AP. Pairing and TFT MAC configuration are
not required, and multiple displays may receive the same frames.

ESP-NOW and the SoftAP must share a radio channel. Both currently use channel 1
through `Config::kWifiAccessPointChannel`. If that value changes, rebuild and
flash both the gate and every TFT. Frames are broadcast and unencrypted; they
must not contain secrets.

## Backlight

This specific panel was tested with `BL` connected directly to GPIO5 as a
3.3 V logic signal. LOW reduced/disabled the backlight and HIGH produced maximum
brightness, confirming active-high control on the tested board.

Firmware configures LEDC channel 0 at 20 kHz with 8-bit resolution:

```cpp
setBacklightBrightness(100); // valid range: 0..100
```

The function clamps values above 100. Startup brightness is selected by
`kDefaultBacklightPercent`, currently 100.

GPIO5 is not a power supply. TFT `VCC` remains connected to USB 5V/VBUS, and the
TFT and ESP32-C3 must share ground. The direct GPIO5 connection applies only to
the tested module whose onboard BL control circuit was confirmed experimentally.

## Build and upload

```sh
pio run -e tft-hardware-test
pio run -e tft-hardware-test -t upload
pio device monitor -b 115200
```

If multiple serial devices are connected, specify the TFT port:

```sh
pio run -e tft-hardware-test -t upload --upload-port /dev/cu.usbmodemXXX
```

Full wiring is in [Hardware](HARDWARE.md), and connection diagnostics are in
[Troubleshooting](TROUBLESHOOTING.md).

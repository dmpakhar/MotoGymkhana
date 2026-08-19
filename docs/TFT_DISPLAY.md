# TFT display and ESP-NOW

## Purpose

The optional display node is an ESP32-C3 connected to a generic 3.5-inch
ST7796S panel. It is display-only: timing, riders, history, and sensor decisions
remain on the ESP32-S3 gate controller.

The PlatformIO environment is named `tft-hardware-test` for compatibility with
the original hardware test, but it now builds `src/tft/GymioTftDisplay.cpp`, the
live Gymio display application. A second environment, `tft-c3-mini-1`, selects
the new ESP32-C3-MINI-1 wiring profile.

| Profile | Environment | Pins (SCK/MOSI/CS/DC/RST/BL) | SPI |
| --- | --- | --- | --- |
| ESP32-C3 Super Mini | `tft-hardware-test` | 4/6/7/3/10/5 | 40 MHz |
| ESP32-C3-MINI-1 | `tft-c3-mini-1` | 5/6/2/4/8/0 | 20 MHz |

## Display behavior

The landscape 480x320 screen shows:

- timer state;
- current or final time in `MM:SS.mmm` format;
- rider assigned to the current attempt (`NONAME` when unassigned);
- absolute valid best time and its rider.

At boot, `WAITING` is shown until a valid frame arrives. `NO DATA` is shown when
the display previously received data but no frame has arrived for 1.5 seconds.
`RADIO ERROR` means ESP-NOW receiver initialization failed.

Rendering uses write-only SPI at the selected profile's frequency. The timer
uses a small sprite per character and updates only changed characters to
minimize visible refresh.

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

The tested panels accept `BL` as an active-high 3.3 V logic signal. The existing
Super Mini profile uses GPIO5; the new C3-MINI-1 profile uses GPIO0. LOW/0%
disables or dims the backlight and HIGH/100% produces maximum brightness.

Firmware configures LEDC channel 0 at 20 kHz with 8-bit resolution:

```cpp
setBacklightBrightness(100); // valid range: 0..100
```

The function clamps values above 100. Startup brightness is selected by
`kDefaultBacklightPercent`, currently 100.

The selected BL GPIO is not a power supply. TFT `VCC` remains connected to USB
5V/VBUS, and the TFT and ESP32-C3 must share ground. Direct logic control applies
only to a module whose onboard BL control circuit has been confirmed.

## Build and upload

```sh
pio run -e tft-hardware-test
pio run -e tft-hardware-test -t upload
pio run -e tft-c3-mini-1
pio run -e tft-c3-mini-1 -t upload
pio device monitor -b 115200
```

If multiple serial devices are connected, specify the TFT port:

```sh
pio run -e tft-hardware-test -t upload --upload-port /dev/cu.usbmodemXXX
```

Full wiring is in [Hardware](HARDWARE.md), and connection diagnostics are in
[Troubleshooting](TROUBLESHOOTING.md).

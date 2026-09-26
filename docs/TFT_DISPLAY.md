# TFT display and ESP-NOW

The optional display node uses an ESP32-C3-MINI-1 and a 320x480 ST7796S panel.
It is display-only: timing, sensor decisions, riders, and history remain on the
main gate. Build it with `tft-c3-mini-1`.

In landscape orientation the screen shows:

- `READY TO GO` while the timer is ready;
- current or final time in `MM:SS.mmm` while running/result is shown;
- active rider (`NONAME` when unassigned);
- absolute valid best time and its rider;
- `1 GATE` or `2 GATES` according to the connected finish gate;
- connection/timer state.

At boot it shows `WAITING`. `NO DATA` appears if updates stop for 1.5 seconds,
and `RADIO ERROR` indicates ESP-NOW initialization failure. Rendering updates
only changed timer glyphs to minimize TFT flashing.

## ESP-NOW

The main gate broadcasts `DisplayStatusProtocol::Frame` every 100 ms. The TFT
listens in station mode without associating to Wi-Fi, so it consumes neither a
SoftAP client slot nor a DHCP address. No MAC pairing is required. Both nodes
must use `Config::kWifiAccessPointChannel`, currently channel 1.

Frames contain a protocol marker/version, sequence number, timer milliseconds,
timer state, gate count, rider, absolute best time, and absolute-best rider. Broadcasts are
unencrypted and must not contain secrets.

## Cancel button

An optional normally-open push button connects `GPIO20` to `GND`. The firmware
uses the internal pull-up and debounces the active-low input. A press broadcasts
the cancel command five times over ESP-NOW; the main gate deduplicates it and
stops only an active attempt without saving a history entry. Do not connect the
button to 5 V.

## Backlight

The tested TFT accepts `BL` as an active-high 3.3V control input on GPIO0.
Firmware configures 20 kHz, 8-bit LEDC PWM; `setBacklightBrightness(percent)`
accepts 0–100 and startup defaults to 100%. `BL` is not a power input. TFT VCC
must remain on USB/VIN 5V with a common ground.

## Build and upload

```sh
pio run -e tft-c3-mini-1
pio run -e tft-c3-mini-1 -t upload --upload-port TFT_PORT
pio device monitor --port TFT_PORT --baud 115200
```

See [Hardware](HARDWARE.md) for wiring and [Troubleshooting](TROUBLESHOOTING.md)
for connection and display checks.

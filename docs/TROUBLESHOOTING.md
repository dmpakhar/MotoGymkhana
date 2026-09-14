# Troubleshooting

## `gymio.local` does not open

1. Confirm the phone is connected to the `Gymio` Wi-Fi network.
2. Accept the phone's warning that the network has no internet.
3. Open `http://192.168.4.1/main` or `http://192.168.4.1/rider`.
4. Keep the `http://` prefix; Gymio does not serve HTTPS.

mDNS support varies between phones. Failure of `gymio.local` does not imply the
web server is down if the IP address works.

## A phone cannot join Wi-Fi

The SoftAP is configured for at most 15 simultaneous clients. The 20-rider
registry is a storage limit, not a promise that 20 phones can remain connected
at once. Disconnect unused phones and retry. The TFT uses ESP-NOW without AP
association and does not consume one of these client slots.

## TFT remains `WAITING`

- Both gate and TFT must contain the ESP-NOW firmware; changing transport
  requires flashing both devices.
- Both builds must use the same `kWifiAccessPointChannel` value.
- Confirm the gate serial log contains `Display ESP-NOW broadcaster ready.`
- Confirm the TFT serial log contains `Display ESP-NOW receiver ready.`
- Move the devices close together for diagnosis and reboot both.

The TFT does not need the `Gymio` password, an IP address, DHCP, or MAC pairing.

## Start gate stays in `1GM`

- Power the finish gate and wait for its sensor calibration.
- Confirm both gates use the same `kWifiAccessPointChannel` value.
- The finish serial log should contain `ESP-NOW finish link ready`.
- Keep the gates nearby while testing radio connectivity.
- Rebuild and flash both gates after changing `FinishGateProtocol.h`.

The main gate enters `2GM` after receiving finish heartbeats and returns to
`1GM` when they are absent for 1.5 seconds. Mode is latched for an active run,
so a run that began in `2GM` still expects the remote finish event.

## Finish-gate setup does not respond

Remote setup is available only while the timer is calibrated and `READY`.
Confirm the finish gate is online, then enable its setup panel before changing
the threshold. Commands are retried until acknowledged, but incompatible old
firmware cannot acknowledge them.

## TFT shows `NO DATA`

The TFT received frames previously but none for at least 1.5 seconds. Confirm
the gate is powered, the sensor loop is still running, and the channel has not
changed. Restarting only the TFT does not require restarting the gate.

## TFT is blank or grey

- TFT `VCC` must be on stable USB 5V/VBUS, not the ESP32 3.3 V regulator.
- TFT and ESP32-C3 grounds must be common.
- Confirm `IM0=1`, `IM1=1`, and `IM2=1` for 4-wire SPI.
- Verify every signal against the C3-MINI-1 profile in [Hardware](HARDWARE.md).
- Run the serial monitor at 115200 and check for TFT or sprite initialization errors.

## Backlight does not respond

The direct-control result is specific to the tested TFT PCB. Its `BL` pin was
confirmed active-high: 0% is off/dim and 100% is maximum on GPIO0. Do not assume a different panel's BL pin is
logic-level; verify its schematic or test safely.
Never attempt to power a raw backlight from an ESP32 GPIO.

## Timer digits flicker

The current renderer sends only changed glyphs at 20 MHz. Long or poor-quality
SPI wiring can require a lower clock but will lengthen visible updates. Keep SPI
wires short, provide a stable 5 V display supply, and retain a common ground.

## Upload fails or the port is missing

List ports with:

```sh
pio device list
```

Then select one explicitly with `--upload-port`. Close any serial monitor that
already owns the port. On macOS the same board may reappear with a different
`/dev/cu.usbmodem...` number after reconnection.

## Rider registration appears lost

Riders and attempts live in RAM and disappear when the gate restarts. Phone
identity lives in browser local storage. Incognito mode, another browser, or
clearing site data creates another identity.

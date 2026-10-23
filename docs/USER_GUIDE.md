# Gymio user guide

This guide covers normal setup and operation of Gymio. For wiring, firmware
installation, or API integration, see the links at the end of this document.

## System overview

Gymio consists of:

- a **main/start gate**, which owns the timer, rider list, attempt history,
  Wi-Fi network, and operator web page;
- an optional **finish gate**, which enables a separate finish line;
- an optional **large TFT**, which displays the current rider, time, gate mode,
  and absolute best result;
- riders' phones, which connect directly to the main gate over local Wi-Fi.

The gates can be assembled as `Classic` (external/USB power, no battery
subsystem) or `Battery Extended` (1S 18650 power system). This does not change
the controls or software behavior.

## Before an event

1. Place the gates so that a motorcycle crosses the VL53L1X sensor beam.
2. Make sure nothing is standing in either beam.
3. Power the main gate.
4. If used, power the finish gate and large TFT.
5. Wait for the main OLED to finish calibration and show `READY`.
6. Check the mode shown on the main OLED and TFT:
   - `1GM` / `ONE GATE` — one-gate mode;
   - `2GM` / `TWO GATES` — the finish gate is connected.
7. Perform a slow test crossing before starting the event.

Do not stand in front of a sensor during startup calibration. If a gate was
blocked while booting, clear it and restart that gate.

## Powering the system

### Classic

Connect the controller to its normal USB or regulated external supply. Classic
has no 18650, charger, or battery indicator.

### Battery Extended

Battery Extended has two power controls:

- a **master ground switch**, which physically disconnects the battery-powered
  device circuit;
- a momentary **power button** connected to the boost/power controller.

To start the gate:

1. Turn the master ground switch on.
2. Press the power button once.
3. Wait for the OLED and sensor calibration.

To stop the gate normally, press the power button twice. For transport or
storage, switch the master ground disconnect off after shutdown. If the unit
does not react to the power button, first check that the master ground switch is
on.

Charge through the TP4056 USB-C connector. The current prototype can operate
while connected to USB, but the TP4056 is not a true load-sharing charger; an
active system can affect charge termination. The effect of the master ground
switch on charging depends on the physical wiring, so verify the charger LED
rather than assuming that charging continues with the switch off.

If Battery Extended does not start, charge the cell and inspect the power chain.
Never connect an 18650 directly to an ESP32 pin. Full power wiring and USB-C
charger details are in [Power configurations](POWER.md).

## Connecting a phone

1. Open Wi-Fi settings.
2. Join `Gymio` using password `gymio123`.
3. Accept the phone's warning that this network has no internet.
4. Open one of these addresses:
   - operator page: `http://gymio.local/main`;
   - rider page: `http://gymio.local/rider`.
5. If `.local` does not open, use:
   - `http://192.168.4.1/main`;
   - `http://192.168.4.1/rider`.

Remain connected to `Gymio` while using the pages. Some phones automatically
switch back to mobile data or another Wi-Fi network when internet is absent;
disable that behavior if the page repeatedly becomes offline.

## Rider instructions

### First registration

1. Open the rider page.
2. Enter your rider name.
3. Press **Register**.

Registration associates the rider with an identifier stored by that browser.
It does not prepare the rider automatically.

### Preparing for a ride

1. Open the rider page from the registered phone.
2. Press **Prepare for ride**.
3. Confirm that **Current attempt** shows your name.
4. Wait for `READY`, then begin the attempt.

`NONAME` means no registered rider is assigned. Preparing another rider replaces
the currently prepared rider unless an attempt is already running. Once an
attempt starts, its rider is locked until it finishes or is cancelled.

After a valid finish, the personal page shows the rider's best result and up to
five latest valid results.

### Deleting a registration

Press **Delete rider** and confirm. The rider is removed from the active rider
list and the phone becomes unregistered. Completed attempts and their recorded
names remain in operator history. A rider cannot delete their registration
during their active attempt.

Use the same browser on later visits. Incognito mode, clearing site data, or
using another browser creates a different phone identity and requires another
registration. Gymio stores up to 20 registered riders.

## Operator page

The operator page is available at `/main` and provides:

- live timer state and current rider;
- rider creation, selection, and deletion;
- absolute best time and its rider;
- the latest 20 attempts;
- attempt reassignment, invalidation, and revalidation;
- minimum valid time configuration;
- start- and finish-sensor setup;
- current-attempt cancellation.

### Selecting or adding a rider

Press a rider's name to assign that rider to the next attempt. Select `ANON` for
an unnamed attempt. Enter a new name and press **Add** to create and select an
operator-managed rider.

Press `×` beside a rider and confirm to remove the registration. Historical
attempts are preserved.

### Correcting attempt history

- Press an attempt's rider name or **Reassign** to give that result to another
  registered rider.
- Use **Invalidate** to exclude a result from personal and absolute bests.
- Use **Revalidate** to make a manually invalidated result eligible again.

Reassignment preserves the attempt number, duration, and validity. If the
changed attempt is the absolute best, the displayed best rider is recalculated.

### Minimum valid time

Open **Time validation**, enter the shortest plausible course time in seconds,
and press **Save**. Set `0` to disable automatic filtering.

A shorter attempt remains visible as `BELOW MINIMUM`, but is excluded from
personal and absolute best results. The setting survives controller restarts
and reevaluates all attempts still present in the 20-entry history.

### Cancelling an attempt

During a running attempt, press **Stop current attempt** and confirm. The timer
returns to its idle flow without saving a history entry.

On compact S3 Mini start and finish gates, a short press of the physical gate
button does the same. The optional button on the large TFT also sends the same
cancellation command. These controls have no effect when an attempt is not
running.

## Timer operation

The normal sequence is:

1. `READY` — waiting for the prepared rider to cross the start gate.
2. `CLEAR GATE` — timing has started; move fully out of the sensor beam.
3. `RUNNING` — the start gate is clear and the timer is armed for the finish.
4. `RESULT` — the completed time is displayed for five seconds.
5. Gymio returns to `READY`; the next attempt defaults to unnamed.

In one-gate mode, `CLEAR GATE` remains active for at least 1.5 seconds after
the start. This is intentional: the entire motorcycle, including the rear
wheel, must pass before the same gate becomes eligible to detect the finish.

### One-gate mode (`1GM`)

Cross the main gate to start, clear its beam, ride the course, and cross the
same gate again to finish.

### Two-gate mode (`2GM`)

Cross the main gate to start and the connected finish gate to finish. The mode
is locked when the attempt begins, so temporary radio changes cannot turn a
two-gate attempt into a one-gate finish.

When idle, the main gate enters `2GM` automatically after receiving finish-gate
heartbeats. If the finish gate is absent for about 1.5 seconds, it returns to
`1GM`. Do not start until the displayed mode matches the course layout.

The finish gate sends a crossing immediately over ESP-NOW. Its 100 ms periodic
heartbeat repeats the crossing counter as a fallback if the immediate packet is
lost.

## Gate setup

Changing a gate threshold blocks new attempts. Set the target at the physical
distance where an object should count as crossing; a measured distance at or
below the saved limit triggers the gate.

### Compact main gate button

This procedure applies to the `main-gate-s3-mini` hardware:

1. Wait until the calibrated timer is `READY`.
2. Hold the gate button for 1.5 seconds.
3. Confirm that the OLED shows `GATE SETUP` and a live distance.
4. Place a target at the desired trigger distance.
5. Short-press the button to save the displayed distance.
6. Confirm `GATE SET` and the saved distance on the OLED for three seconds.
7. Remove the target completely. Attempts remain blocked until the beam clears.

The saved threshold is normalized from the rolling-average reference distance:
Gymio subtracts a 150 mm safety margin so normal sensor jitter does not create
false crossings. An `INF` reference therefore saves 3850 mm. Outside setup
mode, a short press cancels a running attempt instead.

### Main gate from the operator page

1. Wait for `READY`.
2. Open **Sensor debug**.
3. Press **Enable Sensor Setup** and confirm.
4. Either enter a threshold in millimetres and press **Set limit**, or place a
   target and press **Use current**.
5. Press **Finish Sensor Setup**.
6. Remove the target and verify that the gate is ready.

### Finish gate from the operator page

1. Power the finish gate and wait until the page shows it connected.
2. Wait for the main timer to be `READY`.
3. Open **Finish gate** and press **Enable Finish Setup**.
4. Enter the limit or use the current finish-sensor reading.
5. Press **Finish Setup**, remove the target, and confirm `2GM`.

The finish gate can also be configured locally with its GPIO8 button. After its
own sensor calibration, hold for 1.5 seconds to enter `GATE SETUP`, place the
target, and short-press to save. The main gate does not need to be powered for
this local setup. Remove the target before use. Its blue/yellow/green LEDs mean
running, crossing, and ready, respectively.

## Status LEDs on the compact main gate

| LED | Meaning |
| --- | --- |
| Green | Calibrated and ready |
| Yellow | Gate crossing detected; shown briefly |
| Blue | Attempt in progress |

The green LED is off during gate setup and while Gymio waits for the setup
target to be removed.

## OLED and large TFT

The main OLED shows startup/calibration, `1GM` or `2GM`, rider, live state,
distance, running time, result, and physical gate-setup messages.

The optional large TFT starts at `WAITING`, then shows `READY TO GO`, current
rider, current time, gate mode, absolute best time, and best rider. It uses
ESP-NOW and does not consume a phone Wi-Fi client slot. `NO DATA` means no main
gate update has arrived for more than about 1.5 seconds.

## Firmware updates over Wi-Fi

Open **Configuration** on the main page to connect the main gate to a home
Wi-Fi or phone hotspot. The `Gymio` network stays active at the same time. The
page shows upstream connection state, assigned IP, and the shared radio channel.
Saved credentials survive restart; **Forget home Wi-Fi** returns the system to
local-only operation.

All devices must first receive an OTA-capable release over USB. For later
updates, connect the operator phone or computer to `Gymio`, ensure the timer is
`READY`, and open `http://gymio.local/update`.

No login is required by default. Configuration contains an optional
**Require administrator login** switch. When enabled, Configuration and OTA use
HTTP Basic authentication; defaults are `admin` / `gymio-update`.

On the update page, **Check for updates** queries the latest public GitHub
Release. When a newer version exists, **Install update** downloads and validates
the main-gate image, writes the inactive OTA slot, and restarts automatically.
The timer must be `READY`. Version 0.4.1 must be installed once using USB or the
manual upload form before this one-button path is available for later releases.

- Main-gate firmware uploads directly on the update page.
- For the finish gate, press **Prepare finish gate**, wait about three seconds,
  and open `192.168.4.2/update`.
- For the TFT, press **Prepare TFT display**, wait about three seconds, and open
  `192.168.4.3/update`.

Select only the `firmware.bin` built for that device. Gymio also checks the
embedded target identity and rejects mismatched images. Do not remove power
during an upload. A successful device restarts automatically; an unused remote
maintenance page closes after three minutes.

## Data retention and limits

- Up to 20 registered riders are kept.
- Only the latest 20 attempts remain in history.
- Rider registrations and attempt history are held in RAM and are lost when
  the main gate restarts.
- The minimum-valid-time setting is stored in flash and survives restarts.
- Up to 15 phones can be connected to the Gymio Wi-Fi access point at once.
- Normal operator controls have no authentication. Configuration and OTA can
  optionally use HTTP Basic authentication.

## Quick troubleshooting

### The display remains in calibration

Clear the sensor beam and restart the affected gate. Check VL53L1X power and
I2C wiring if calibration never progresses.

### The page does not open

Confirm the phone is still connected to `Gymio`, then use `192.168.4.1` instead
of `gymio.local`. Disable automatic switching away from networks without
internet.

### The system shows `1GM` with the finish gate powered

Wait several seconds, then check finish-gate power and its OLED. Both gates must
run firmware configured for the same ESP-NOW channel.

### A fast crossing is missed

Verify sensor alignment and threshold. The beam must intersect a stable part of
the motorcycle rather than a narrow gap. Use Sensor Setup and test several fast
passes before the event. Production gate profiles use a 20 ms ToF timing budget
and accept a single reading at least 100 mm inside the threshold immediately.

### The timer remains at `CLEAR GATE`

Something is still inside the sensor zone, or the threshold is too far away.
Clear the beam; if needed, configure the threshold again.

### The TFT says `NO DATA`

The TFT is powered but is not receiving the main gate's ESP-NOW frames. Check
that the main gate is running and both firmware targets use the same radio
channel.

For detailed diagnosis, see [Troubleshooting](TROUBLESHOOTING.md).

## Related documentation

- [Hardware and wiring](HARDWARE.md)
- [Classic and Battery Extended power](POWER.md)
- [Build and flash](BUILD.md)
- [TFT display](TFT_DISPLAY.md)
- [HTTP API](API.md)
- [Troubleshooting](TROUBLESHOOTING.md)

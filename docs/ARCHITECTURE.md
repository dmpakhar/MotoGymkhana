# Architecture

Gymio uses a cooperative Arduino loop. No application tasks or dynamic queues are
created; the web server and sensor are serviced on every pass through `loop()`.

## Components

| Component | Responsibility |
| --- | --- |
| `ToFSensor` | Initializes VL53L1X ranging and returns ready samples. |
| `GateDetector` | Calibrates the clear-gate baseline, filters samples, and emits crossing/clear events. |
| `SingleGateTimer` | Implements the ready, clear-gate, armed, and result state machine. |
| `Display` | Renders startup, calibration, distance, active timer, result, and errors. |
| `RiderRegistry` | Stores up to 20 active riders and browser UUID associations. |
| `MotoWebServer` | Runs the AP-facing HTTP UI/APIs, attempt ring buffer, sensor setup, and ESP-NOW display broadcast. |
| `FinishGate` | Measures the remote finish, displays sensor/link state, and exchanges reliable status/commands over ESP-NOW. |
| `GymioTftDisplay` | Receives ESP-NOW status and renders the large ST7796S UI on an ESP32-C3. |
| `DisplayStatusProtocol` | Defines the versioned binary frame shared by the gate and TFT. |
| `FinishGateProtocol` | Defines finish heartbeat/crossing state and acknowledged setup commands. |
| `GateControlProtocol` | Defines deduplicated TFT-to-main-gate control commands. |

## Measurement and timer flow

```text
VL53L1X sample
      |
      v
GateDetector -- filtered distance --> OLED / web status
      |
      +-- CrossingConfirmed / GateCleared
                         |
                         v
                  SingleGateTimer
                         |
              Started / Finished / Reset
                         |
                         v
             rider history, OLED, web UI
                         |
                    ESP-NOW broadcast
                         |
                         v
                  ESP32-C3 TFT display
```

The gate detector normalizes a no-target reading to 4000 mm. It calibrates from
20 clear-gate samples, maintains a five-sample moving average, and initially
sets the trigger 150 mm closer than the calibrated baseline. Entry detection
uses raw distance, preventing a fast crossing from being diluted by the moving
average. The sensor uses a 20 ms timing budget (about 50 samples per second).
A valid reading at least 100 mm inside the configured threshold is accepted in
one sensor frame; marginal readings require 20 ms confirmation.
Release detection uses the filtered distance and must remain stable for 250 ms,
preventing a single far or no-target sample from arming the finish while the
gate is still occupied. In one-gate mode, the timer additionally waits at least
1500 ms after the start before accepting the stable clear state. This keeps the
clear space between the motorcycle's wheels from arming the rear wheel as a
finish. Two-gate mode does not apply this start holdoff.

## Optional finish gate

The ESP32-S3 Mini N4R2 finish gate broadcasts an ESP-NOW heartbeat every
100 ms on the Gymio access-point channel. A crossing also triggers an immediate
frame instead of waiting for the next heartbeat. The frame includes a boot
identifier, sequence, persistent crossing counter, sensor state, and distance.
Repeating the crossing counter in every heartbeat prevents a lost immediate
packet from losing a finish event; the retry delay is at most about 100 ms.

The main gate enters two-gate mode while finish heartbeats are no more than
1500 ms old. The mode is latched when an attempt starts: a two-gate attempt can
only finish from the remote finish sensor, while a single-gate attempt retains
the original local return-crossing behavior. When idle, loss of the finish
heartbeat automatically restores single-gate mode.

## Rider identity

The rider page generates a browser UUID and stores it under `gymioClientId` in
local storage. Registration sends that UUID to the ESP32, where it is associated
with one active rider. Preparation globally selects that rider for the next
attempt. At start, a copy of the rider is locked to the attempt so later UI
changes cannot reassign a running attempt.

The firmware stores 20 attempts in a ring buffer. Each attempt contains a rider
snapshot, duration, sequence number, and validity flag. This preserves historical
names and times after an active rider registration is deleted.

Only valid attempts participate in personal or absolute best calculations.
Reassigning an attempt changes its rider snapshot; if that attempt is the best,
the displayed best rider changes as well. Invalidation excludes an attempt from
best calculations without deleting it.

The persisted minimum-valid-time setting adds automatic validity normalization.
An attempt remains in history but is ineligible whenever its duration is below
the current threshold. Manual validity is tracked independently, so changing
the threshold cannot restore an attempt that the operator manually invalidated.

## Radio architecture

The main ESP32-S3 runs in `WIFI_AP_STA` mode. Its WPA2 SoftAP named `Gymio`
remains available to phones and remote modules while its station interface can
join an internet-connected home Wi-Fi or phone hotspot. When STA connects, the
single ESP32 radio makes the SoftAP and ESP-NOW use the upstream access point's
channel. Finish and TFT modules automatically scan channels after losing main
gate frames, then remain on the discovered channel.

Home-network credentials and the optional administrator-authentication switch
are stored in the main gate's NVS. Loss of upstream Wi-Fi does not stop the
local AP, timer, web UI, or OTA upload path.

## Firmware CI/CD

`.github/workflows/firmware.yml` builds all supported environments on every
main-branch push and pull request. A `v*` tag creates a GitHub Release containing
target-named binaries. Every binary retains its embedded `GYMIO_TARGET` marker,
which the receiving device validates before committing an OTA update.

## OTA maintenance path

The main gate exposes an authenticated `/update` page on its SoftAP. Main-gate
images stream directly into the inactive OTA application partition. For a
remote update, the main gate repeats a target-specific ESP-NOW maintenance
command for five seconds. The selected endpoint temporarily associates with
the same Gymio AP and starts an authenticated updater at a reserved address:
finish gate `192.168.4.2`, TFT `192.168.4.3`.

Remote maintenance mode stops after three minutes when no upload is active.
Normal finish/TFT operation remains ESP-NOW-only outside that window. Every
production build embeds `GYMIO_TARGET=<environment>`; the receiving updater
scans the upload and calls `Update.end()` only when that target matches. OTA
writes the inactive `ota_0`/`ota_1` slot, preserving the running image when an
upload fails or is rejected.

The TFT normally enables station mode only to use the radio, fixes its channel
to the same configured value, and does not associate with the AP or obtain an
IP address. It associates temporarily only during requested OTA maintenance.
Frames use the broadcast MAC address, so the gate does not need the TFT's MAC
and multiple displays can listen. Status traffic is one-way and unencrypted.
See [TFT display and ESP-NOW](TFT_DISPLAY.md).

## HTTP surface

| Method | Path | Purpose |
| --- | --- | --- |
| GET | `/`, `/main` | Main/operator dashboard. |
| GET | `/rider` | Personal rider page. |
| GET | `/configuration` | Main-gate network, security, and firmware configuration. |
| GET/POST/DELETE | `/api/configuration/*` | Read or change persisted main-gate settings. |
| GET | `/update` | Firmware maintenance page; authentication is optional. |
| POST | `/update/main` | Authenticated main-gate OTA upload. |
| POST | `/api/ota/prepare` | Start finish/TFT OTA maintenance mode. |
| GET | `/api/status` | Timer, sensor, rider, and firmware-version status. |
| GET | `/api/history` | Newest-first attempt history. |
| GET/POST/DELETE | `/api/riders` | List, create, or remove riders. |
| POST | `/api/riders/select` | Operator selection. |
| POST | `/api/riders/prepare` | Prepare the phone-associated rider. |
| POST | `/api/riders/logout` | Delete the phone-associated rider registration. |
| POST | `/api/attempts/invalidate` | Exclude an attempt from valid results. |
| POST | `/api/attempts/revalidate` | Restore an attempt to valid results. |
| POST | `/api/attempts/assign` | Reassign a completed attempt to a rider. |
| POST | `/api/settings/minimum-valid-time` | Persist the automatic minimum valid duration. |
| POST | `/api/sensor-setup` | Toggle sensor setup mode. |
| POST | `/api/gate/limit` | Set an explicit threshold. |
| POST | `/api/gate/limit/current` | Use the current raw reading as threshold. |
| POST | `/api/finish-gate/sensor-setup` | Toggle remote finish setup. |
| POST | `/api/finish-gate/limit` | Set the remote finish threshold. |
| POST | `/api/finish-gate/limit/current` | Use the remote finish reading as threshold. |

Operator controls remain local and unauthenticated. Administration and OTA
authentication is disabled by default and can be enabled from Configuration.
Request fields and response shapes are documented in [HTTP API](API.md).

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
average. A valid reading at least 300 mm inside the configured threshold is
accepted in one sensor frame; marginal readings retain a 50 ms confirmation.
Release detection uses the filtered distance and must remain stable for 250 ms,
preventing a single far or no-target sample from arming the finish while the
gate is still occupied.

## Optional finish gate

The ESP32-S3 Mini N4R2 finish gate broadcasts an ESP-NOW heartbeat every
250 ms on the Gymio access-point channel. The frame includes a boot identifier,
sequence, persistent crossing counter, sensor state, and
distance. Repeating the crossing counter in every heartbeat prevents a single
lost radio packet from losing a finish event.

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

The ESP32-S3 runs a WPA2 SoftAP named `Gymio` on the fixed channel in
`Config::kWifiAccessPointChannel` (currently channel 1). Phones join this AP for
HTTP access. The gate also broadcasts a compact status frame every 100 ms using
ESP-NOW on the AP interface.

The TFT enables station mode only to use the radio, fixes its channel to the
same configured value, and does not associate with the AP or obtain an IP
address. Frames use the broadcast MAC address, so the gate does not need the
TFT's MAC and multiple displays can listen. The link is one-way and
unencrypted. See [TFT display and ESP-NOW](TFT_DISPLAY.md).

## HTTP surface

| Method | Path | Purpose |
| --- | --- | --- |
| GET | `/`, `/main` | Main/operator dashboard. |
| GET | `/rider` | Personal rider page. |
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

The HTTP server is intentionally local and unauthenticated in version 0.2.0.
Request fields and response shapes are documented in [HTTP API](API.md).

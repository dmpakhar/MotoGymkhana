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
| `GymioTftDisplay` | Receives ESP-NOW status and renders the large ST7796S UI on an ESP32-C3. |
| `DisplayStatusProtocol` | Defines the versioned binary frame shared by the gate and TFT. |

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
20 clear-gate samples, maintains a five-sample moving average, and initially sets
the trigger 150 mm closer than the calibrated baseline. Crossing and release must
remain stable for 150 ms and 250 ms respectively.

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
| POST | `/api/sensor-setup` | Toggle sensor setup mode. |
| POST | `/api/gate/limit` | Set an explicit threshold. |
| POST | `/api/gate/limit/current` | Use the current raw reading as threshold. |

The HTTP server is intentionally local and unauthenticated in version 0.1.0.
Request fields and response shapes are documented in [HTTP API](API.md).

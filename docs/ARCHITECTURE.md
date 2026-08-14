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
| `MotoWebServer` | Runs the AP-facing HTTP UI, APIs, attempt ring buffer, and sensor setup controls. |

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
             rider history and displays
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
| POST | `/api/sensor-setup` | Toggle sensor setup mode. |
| POST | `/api/gate/limit` | Set an explicit threshold. |
| POST | `/api/gate/limit/current` | Use the current raw reading as threshold. |

The HTTP server is intentionally local and unauthenticated in version 0.1.0.

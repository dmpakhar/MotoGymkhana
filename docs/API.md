# HTTP API

The API is served by the gate at `http://gymio.local` and
`http://192.168.4.1`. It is local, unauthenticated, and intended for the bundled
web pages. Mutating requests use URL-encoded form fields.

## Status and history

| Method | Path | Description |
| --- | --- | --- |
| `GET` | `/api/status` | Firmware version, sensor state, timer state/time, active rider, personal best, and absolute best. |
| `GET` | `/api/history` | Up to 20 attempts, newest first, including number, duration, rider snapshot, and validity. |

Timer states are `CALIBRATING`, `READY`, `CLEAR GATE`, `RUNNING`, and `RESULT`.
Times are unsigned milliseconds. A best time of zero means no eligible result.

## Riders

| Method | Path | Fields | Description |
| --- | --- | --- | --- |
| `GET` | `/api/riders` | Optional `clientId` query | Lists ANON and registered riders, selected/prepared ID, phone rider ID, best, and five recent valid times. |
| `POST` | `/api/riders` | `name`, optional `clientId` | Creates a rider. Operator-created riders are selected immediately; phone registrations are not prepared automatically. |
| `DELETE` | `/api/riders` | `id` query | Removes a registered rider while preserving attempt snapshots. |
| `POST` | `/api/riders/select` | `id` | Selects the next rider from the main page. ID zero selects ANON. |
| `POST` | `/api/riders/prepare` | `clientId` | Prepares the rider registered to that browser; allowed only in READY state outside Sensor Setup. |
| `POST` | `/api/riders/logout` | `clientId` | Deletes the phone-associated rider; blocked during that rider's active attempt. |

Names are 1–24 characters after normalization and may contain letters, digits,
spaces, hyphens, and underscores. Consecutive/outer spaces are normalized.
There can be at most 20 registered riders. A browser client ID must be 8–40
characters and contain only letters, digits, hyphens, or underscores.

## Attempts

| Method | Path | Fields | Description |
| --- | --- | --- | --- |
| `POST` | `/api/attempts/invalidate` | `number` | Excludes an attempt from personal and absolute best calculations. |
| `POST` | `/api/attempts/revalidate` | `number` | Makes an attempt eligible again. |
| `POST` | `/api/attempts/assign` | `number`, `riderId` | Changes the rider snapshot for a completed attempt. |

Reassignment preserves duration, attempt number, and validity. Best times and
the absolute-best rider are derived from current valid history, so they update
after reassignment or validity changes.

## Sensor setup

| Method | Path | Fields | Description |
| --- | --- | --- | --- |
| `POST` | `/api/sensor-setup` | None | Toggles setup mode while the calibrated timer is READY. |
| `POST` | `/api/gate/limit` | `limit` | Sets a threshold from 0 through 3999 mm while setup mode is active. |
| `POST` | `/api/gate/limit/current` | None | Uses the latest raw reading as the threshold while setup mode is active. |

Sensor Setup blocks attempts until it is disabled. Successful setters return a
small JSON confirmation. Client errors use JSON with an `error` string and an
appropriate 4xx status.

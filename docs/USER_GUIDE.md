# User guide

## Starting Gymio

Power the ESP32 and wait for sensor calibration. Keep the gate clear during this
phase. The OLED and web dashboard show when the timer becomes ready.

Connect a phone or computer to the `Gymio` Wi-Fi network. The development
password is `gymio123`. Gymio is a local network without internet access, so a
phone may warn that the network has no internet.

## Main dashboard

Open `http://gymio.local/main` or `http://192.168.4.1/main`.

The main dashboard provides:

- current timer state, time, and assigned rider;
- rider creation, selection, and deletion;
- session best for the selected or active rider;
- the latest 20 attempts, including invalidate and revalidate controls;
- sensor distance, filtered distance, and gate threshold;
- Sensor Setup mode for safely changing the threshold while attempts are blocked.

Deleting a rider removes the registration but does not remove completed entries
from attempt history. `ANON` is the operator-side fallback when nobody is assigned.

## Personal rider page

Open `http://gymio.local/rider` or `http://192.168.4.1/rider`.

On first use, enter a name and press **Register**. Gymio associates the generated
browser UUID with that rider. Registration does not reserve the next attempt.

Before riding, press **Prepare for ride**. All rider pages then show that rider
under **Current attempt**. `NONAME` means nobody is prepared. A newly prepared
rider replaces the previous prepared rider unless an attempt is already active.

After a valid finish, the rider page shows the personal best and up to five most
recent valid results. Press **Delete rider** and confirm to remove the registration;
completed history entries remain available on the main dashboard. Deletion is
blocked during that rider's active attempt.

Use the same browser for repeat visits. Incognito mode, a different browser, or
cleared site data produces a different phone UUID.

## Timer sequence

1. **READY** — waiting for the prepared rider to cross the gate.
2. **CLEAR GATE** — timer is running; rider must leave the sensor zone.
3. **RUNNING** — gate is armed for the return crossing.
4. **RESULT** — elapsed time is displayed for five seconds.
5. The timer returns to **READY**, and the next attempt defaults to anonymous.

## Sensor setup

Sensor Setup is available from the main dashboard only when the timer is
calibrated and ready. While active, attempts are blocked. Set a threshold in
millimetres or use the current raw distance, then finish Sensor Setup before use.

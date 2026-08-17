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
- absolute best time and its rider;
- the latest 20 attempts, including reassignment, invalidate, and revalidate controls;
- sensor distance, filtered distance, and gate threshold;
- Sensor Setup mode for safely changing the threshold while attempts are blocked.

Click an attempt's rider name or **Reassign** to assign that completed time to a
different registered rider. The absolute best rider is recalculated
automatically. Deleting a rider removes the registration but does not remove
completed entries from attempt history. `ANON` is the internal/operator fallback;
the rider-facing UI and TFT show `NONAME` when nobody is assigned.

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

Gymio can store 20 named riders. This is separate from the gate's limit of 15
simultaneously connected Wi-Fi clients. The TFT does not join the Wi-Fi network
and therefore does not consume a client slot.

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

## Large TFT display

The optional TFT shows the timer state, current time, active rider, absolute
best time, and best rider. It starts in `WAITING`, changes to live timer state
after receiving ESP-NOW data, and shows `NO DATA` if updates stop for more than
1.5 seconds. It does not provide controls; rider and attempt management remains
on the web pages.

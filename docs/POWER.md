# Gymio power configurations

Gymio gate electronics can be assembled in either of two power configurations.
This choice changes only the power subsystem: the controller profile, OLED,
VL53L1X, controls, networking, and firmware behavior remain the same.

## Classic

`Classic` has no battery, charger, boost converter, or battery measurement
circuit. Power the controller from USB or from a suitable regulated input for
the particular development board. The controller's regulated 3.3 V output
powers the SSD1306 OLED and VL53L1X.

Do not connect an 18650 cell directly to an ESP32 power pin. Follow the voltage
limits printed on the exact controller board when using an external supply.

## Battery Extended

`Battery Extended` adds the verified self-contained 1S power subsystem:

- one 18650 Li-ion cell (about 3.7 V nominal, 4.2 V fully charged);
- a TP4056-based USB-C 1S charger/protection module (ZX-056 style);
- the existing boost/power-controller module, which produces approximately
  5 V and provides device power on/off control;
- a 1N5819 Schottky isolation diode before the ESP32 5 V input.

```text
USB-C ----> TP4056 charger/protection
                 ^          |
                 |          | OUT+ / OUT-
             1S 18650       v
                       boost / power controller
                                  |
                             about 5 V
                                  |
                                1N5819
                                  |
                                  v
                            ESP32 5 V input

ESP32 3.3 V ----> SSD1306 + VL53L1X
All grounds are common.
```

### Battery and protected output wiring

| From | To | Purpose |
| --- | --- | --- |
| 18650 positive | TP4056 `B+` | Cell positive |
| 18650 negative | TP4056 `B-` | Cell negative |
| TP4056 `OUT+` | Boost-controller input positive | Protected supply |
| TP4056 `OUT-` | Boost-controller input negative | Protected return |
| Boost 5 V output | 1N5819 anode | Isolated controller supply |
| 1N5819 cathode/striped end | ESP32 5 V input | Stripe faces ESP32 |

The diode prevents USB 5 V on the ESP32 from feeding back into the boost
controller. Do not add a second path from ESP32 5 V to the boost-controller
input: that experimental loop caused repeated controller resets.

### USB-C CC modification on the tested TP4056 board

The inexpensive charger originally charged from USB-A-to-USB-C cables but not
from USB-C-to-USB-C supplies. On the tested board, the six connector contacts
were identified as follows:

| Tested-board contact | Signal |
| --- | --- |
| 1 and 6 | GND |
| 2 and 5 | VBUS +5 V |
| 3 | CC1 |
| 4 | CC2 |

Fit one 5.1 kΩ pull-down from CC1 to GND and a separate 5.1 kΩ pull-down from
CC2 to GND. Do not connect CC1 and CC2 directly together. This modification was
physically verified with both USB-A-to-C and USB-C-to-C charging.

This contact numbering describes only the tested ZX-056-style board. Cheap
modules and connector footprints vary; identify contacts with the actual board
unpowered before modifying another revision. Never solder to an unidentified
USB-C contact.

Verified components used in the prototype:

- two RCL R3511 5.1 kΩ, 0.25 W through-hole resistors for CC1/CC2;
- one 1N5819, 1 A, 40 V Schottky diode (DO-41).

## Verified status

The following have been tested together on the hardware prototype:

- TP4056 charging and battery protection;
- charging from USB-A-to-C;
- USB-C-to-C charging after fitting the two independent CC resistors;
- protected `OUT+`/`OUT-` powering the boost/power controller;
- boost output powering the ESP32 through the Schottky diode;
- the complete power chain operating correctly.

## Known limitation

TP4056 is not a true power-path/load-sharing charger. The prototype can operate
while USB is connected, but the active load may affect charge termination. This
is accepted for `Battery Extended`; a future PCB may replace it with a proper
charger and power-path design.

## Planned battery measurement

Battery voltage sensing is planned, not part of the current production pinout.
No ADC GPIO is assigned yet.

The proposed divider is:

```text
Battery B+ ---- 100 kΩ ----+---- ESP32 ADC (TBD)
                            |
                          100 kΩ
                            |
                           GND

Optional: 100 nF ceramic capacitor from the ADC node to GND.
```

The 1:2 divider gives approximately 2.10 V at the ADC for a 4.20 V cell.
Firmware must calibrate the ADC under real load and use a non-linear Li-ion
voltage curve; this circuit is only an approximate state-of-charge indicator.


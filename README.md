# StreetLight SmartNode: Fault and Energy Monitor

**Innovators Conclave 2026 | Smart Cities & CivilTech | IC26-04-08 (Hardware)**
**Team:** <Safemine> | **Institution:** <G.Pullaiah College Of Engineering And Technology>

> **Safety:** This prototype uses a 5V low-voltage LED model only. It must never be connected to mains power.

## Problem
Streetlight faults are often discovered only when residents complain, so dark stretches can last for days, raising safety concerns. Lamps left on in daylight or drawing abnormal power also waste energy. Checking thousands of lights manually is slow.

## Solution
StreetLight SmartNode monitors a low-voltage model street-lighting circuit. It measures the supply current and light level, decides whether the lamps are behaving correctly, and reports likely faults and energy used on a local display and serial log.

### Key features
- Automatic dusk-to-dawn lamp control using a light sensor
- Lamp-fault detection by comparing measured current against the expected current
- Abnormal-power detection (current far above expected)
- Daylight energy-waste flag
- Stuck-on detection (lamps commanded off but current still flowing)
- Energy log in mWh
- OLED status display, LED and buzzer alerts, CSV serial log
- Planned: Wi-Fi dashboard listing each pole's status

## How it works
1. An INA219 sensor measures voltage and current on the lamp supply line.
2. An LDR detects darkness, and the ESP32 switches the lamp bank through a transistor.
3. One working lamp draws a calibrated current (`I_LAMP_MA`). Estimated working lamps = measured current / lamp current.
4. The ESP32 compares this with how many lamps should be on:
   - Fewer working lamps than expected gives **LAMP FAULT**
   - Current far above expected gives **HIGH POWER**
   - Lamps on in daylight gives **DAY WASTE**
   - Lamps commanded off but drawing current gives **STUCK ON**
5. Power is integrated over time to give energy in mWh.

## Hardware
| Part | Purpose |
|------|---------|
| ESP32 dev board | Controller, Wi-Fi |
| INA219 current sensor | Voltage and current measurement |
| 4 white LEDs with 100 ohm resistors, toggle switches | Model street lamps and fault simulation |
| LDR with 10k resistor | Day and night detection |
| NPN transistor with 1k resistor | Switches the lamp bank |
| 0.96" I2C OLED (SSD1306) | Status display |
| Red, green, yellow LEDs, buzzer | Alerts |
| 5V power bank | Power |

Wiring is documented at the top of `streetlight_smartnode.ino`.

## Setup
1. Install Arduino IDE and add ESP32 board support.
2. Install the libraries Adafruit INA219, Adafruit SSD1306 and Adafruit GFX.
3. Measure the current of one lamp and set `I_LAMP_MA` in the code.
4. Read the LDR in light and dark and set `LDR_DARK_BELOW` and `LDR_LIGHT_ABOVE`.
5. Upload, then open the Serial Monitor at 115200 baud.

Test commands over serial: `a` automatic, `f` force lamps on, `o` force lamps off.

## Test scenarios
| Test | How | Expected state |
|------|-----|----------------|
| Normal night | Cover LDR, all 4 lamps on | NORMAL |
| One lamp out | Flip one toggle off | LAMP FAULT (3 of 4) |
| Two lamps out | Flip two toggles off | LAMP FAULT (2 of 4) |
| Abnormal power | Add an extra uncounted LED branch | HIGH POWER |
| Daylight waste | Torch on LDR, send `f` | DAY WASTE |
| Stuck on | Send `o` while the transistor is bypassed | STUCK ON |

Each test is repeated at least three times, and the CSV logs are stored in `/data`. Do not short-circuit the supply.

## Repository structure
```
streetlight-smartnode/
  streetlight_smartnode.ino   Firmware
  README.md                   This file
  /docs                       Block diagram, wiring diagram, model photos
  /data                       Test logs (CSV) and demo video
```

## Team roles
| Member | Role |
|--------|------|
| 1 | Team lead, integration, pitch |
| 2 | Hardware wiring and INA219 setup |
| 3 | Firmware and fault logic |
| 4 | Model street, test scenarios and data |
| 5 | Dashboard and Wi-Fi status |
| 6 | Documentation, diagrams, GitHub, slides |

## 36-hour plan
| Hours | Goal |
|-------|------|
| 0-4 | Wire the INA219 and OLED, verify readings, repo structure |
| 4-10 | Lamp bank, transistor control, calibrate current per lamp |
| 10-16 | Fault logic and alerts, run test scenarios, log data |
| 16-22 | Dashboard, integration, model street build |
| 22-28 | Mentor feedback changes, usability, neat wiring |
| 28-33 | Final testing, bug fixes, demo rehearsal |
| 33-36 | Final commit, backup video, pitch preparation |

## Estimated cost per node
Approximate prototype parts cost: Rs. 1,500 to 2,200. Costs at scale would differ and are not validated.

## Limitations
- One current sensor gives the number of failed lamps, not which lamp.
- Tested on a low-voltage LED model only, not real mains streetlights.
- Real lamps (sodium, LED drivers) behave differently, so calibration and safety work would be needed.
- Current-based detection can be affected by supply variation.
- Any energy-saving figure is an estimate, not a measured result.

## Validation plan
- Calibrate lamp current against a multimeter.
- Repeat each fault scenario several times, recording detection results and false alarms.
- Pilot on a campus lighting circuit with qualified electricians before any real deployment.

## Future scope
- Per-pole current or light sensors for exact fault location
- LoRa wireless reporting for city-wide networks
- Automatic fault tickets into a municipal complaint system
- Dimming schedules and solar-powered nodes

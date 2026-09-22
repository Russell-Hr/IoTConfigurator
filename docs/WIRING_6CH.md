# HeatControl V0.18.7 — 6CH + AUX TEN wiring reference

> **Authoritative GPIO map**
>
> - GPIO4: DS18B20 1-Wire bus (6 room/channel sensors + 1 OUTDOOR sensor)
> - GPIO5: AUX TEN relay control (**ESP32 strapping pin; relay input must not alter the boot-time strap level**)
> - GPIO21: DS3231 SDA
> - GPIO22: DS3231 SCL
> - CH1: GPIO16 / GPIO17
> - CH2: GPIO18 / GPIO19
> - CH3: GPIO23 / GPIO25
> - CH4: GPIO26 / GPIO27
> - CH5: GPIO32 / GPIO33
> - CH6: GPIO13 / GPIO14
>
> AUX is a single controller-level helper output, not CH7. The exact 230 V power wiring, protective devices, relay contact ratings and installation must be selected and commissioned by a qualified electrician.

# HeatControl V0.18.7 — 6-channel wiring architecture

## Scope

This document is the authoritative logical wiring map for the firmware. It is a low-voltage/control architecture document; it is **not** a 230 V installation guide. Mains switching, protection, isolation, enclosure and conductor clearances must be designed and checked by a qualified electrician.

## ESP32 DevKit V1 / WROOM pin map

| GPIO | Function |
|---:|---|
| GPIO4 | 1-Wire DATA — all seven DS18B20 |
| GPIO21 | I²C SDA — DS3231 + EXT GPIO — MCP23017 (optional) |
| GPIO22 | I²C SCL — DS3231 + EXT GPIO — MCP23017 (optional) |
| GPIO16 | CH1 OUT-A |
| GPIO17 | CH1 OUT-B |
| GPIO18 | CH2 OUT-A |
| GPIO19 | CH2 OUT-B |
| GPIO23 | CH3 OUT-A |
| GPIO25 | CH3 OUT-B |
| GPIO26 | CH4 OUT-A |
| GPIO27 | CH4 OUT-B |
| GPIO32 | CH5 OUT-A |
| GPIO33 | CH5 OUT-B |
| GPIO13 | CH6 OUT-A |
| GPIO14 | CH6 OUT-B |

GPIO16/17 are intended here for an ESP32-WROOM/no-PSRAM configuration. Do not reuse this exact map on a board where those pins are occupied by PSRAM.

## Relay mapping

| Relay | GPIO | Channel | Valve mode | Heater mode |
|---:|---:|---:|---|---|
| R1 | 16 | CH1 | OPEN | heater ON/OFF |
| R2 | 17 | CH1 | CLOSE | unused |
| R3 | 18 | CH2 | OPEN | heater ON/OFF |
| R4 | 19 | CH2 | CLOSE | unused |
| R5 | 23 | CH3 | OPEN | heater ON/OFF |
| R6 | 25 | CH3 | CLOSE | unused |
| R7 | 26 | CH4 | OPEN | heater ON/OFF |
| R8 | 27 | CH4 | CLOSE | unused |
| R9 | 32 | CH5 | OPEN | heater ON/OFF |
| R10 | 33 | CH5 | CLOSE | unused |
| R11 | 13 | CH6 | OPEN | heater ON/OFF |
| R12 | 14 | CH6 | CLOSE | unused |

For a valve: OPEN = A on/B off; CLOSE = A off/B on; STOP = both off. Firmware also applies a 100 ms software dead-time when reversing direction. This is not a substitute for suitable hardware interlocking.

## DS18B20 bus

All six DS18B20 share GPIO4. Use the normal 1-Wire topology and a 4.7 kΩ pull-up to 3.3 V. Each sensor is identified by its unique 64-bit ROM address. CH1…CH6 are permanently assigned to heating channels; the seventh sensor is assigned independently as OUTDOOR and is displayed as ambient/outdoor temperature.

```text
ESP32 GPIO4 DATA ──+── DS18B20 CH1
                   +── DS18B20 CH2
                   +── DS18B20 CH3
                   +── DS18B20 CH4
                   +── DS18B20 CH5
                   +── DS18B20 CH6
                   +── DS18B20 OUTDOOR
                   │
                   └── 4.7 kΩ ── +3.3 V
```

## I²C bus

```text
GPIO21 SDA ───── DS3231 SDA
             └─ MCP23017 SDA (optional)
GPIO22 SCL ───── DS3231 SCL
             └─ MCP23017 SCL (optional)
```

The firmware uses MCP23017 address `0x20`. The expander is optional.

## Optional 12 limit-switch inputs

The EXT GPIO module is an MCP23017 and provides 16 GPIOs. Twelve are used for six pairs:

| MCP23017 | Channel | Signal |
|---|---|---|
| GPA0 | CH1 | OPEN limit |
| GPA1 | CH1 | CLOSE limit |
| GPA2 | CH2 | OPEN limit |
| GPA3 | CH2 | CLOSE limit |
| GPA4 | CH3 | OPEN limit |
| GPA5 | CH3 | CLOSE limit |
| GPA6 | CH4 | OPEN limit |
| GPA7 | CH4 | CLOSE limit |
| GPB0 | CH5 | OPEN limit |
| GPB1 | CH5 | CLOSE limit |
| GPB2 | CH6 | OPEN limit |
| GPB3 | CH6 | CLOSE limit |

GPB4–GPB7 remain reserved. Firmware configures the 12 used inputs with pull-ups and treats active-low switches as active by default. The exact electrical interface of the selected limit switches must be verified before connection.

## Valve feedback modes

Every valve channel independently selects:

- `TIME`: full OPEN/CLOSE travel times are configured and position is estimated from elapsed time. No MCP23017 is required.
- `LIMIT_SWITCH`: OPEN and CLOSE endpoints are physically reported by the two limit inputs. MCP23017 must be detected and healthy.

If MCP23017 is absent, the controller remains operational. Only `LIMIT_SWITCH` selection/operation is unavailable. Existing configurations do **not** silently fall back from LIMIT_SWITCH to TIME.

If MCP23017 disappears during LIMIT_SWITCH operation, the affected valve is stopped and reported as a fault. The controller continues operating other channels and non-limit functions. The firmware periodically retries detection.

If both OPEN and CLOSE endpoints for the same channel are simultaneously active, that channel is faulted and movement is stopped.

## AP / software notes

The controller's own Wi-Fi AP password is stored in NVS and can be changed from Web UI or Android. WPA2 AP passwords must be 8–63 characters. The default value in the source is intended for bench use and should be changed before deployment.

The Web UI/API uses HTTP Basic Auth over local HTTP in this prototype. It is not equivalent to HTTPS.

## OUTDOOR sensor

The seventh DS18B20 shares the same GPIO4 1-Wire bus. It is not a seventh heating channel. Its 64-bit ROM is stored separately as `outdoorSensorRom`. The Web UI and Android app display its live temperature on the main dashboard and provide sensor assignment under System. If it is missing, the UI shows `OFFLINE` / `--` rather than treating the reading as a valid zero.

## TIME-mode valve calibration

Calibration is required only for valves using `TIME` feedback when the position is unknown. The UI provides an initial position (`closed` or `open`), `START` and `STOP`. Starting from closed measures the OPEN travel time; starting from open measures the CLOSE travel time. The measured seconds are stored as the corresponding full-travel time. For `LIMIT_SWITCH` valves, time calibration is not required; physical endpoint switches determine the end positions.

## AUX TEN — dedicated controller-wide output

AUX is a single additional ON/OFF heater output and is **not CH7**.

| GPIO | Function |
|---:|---|
| GPIO5 | AUX TEN relay input |

The AUX relay is controlled independently from CH1…CH6. Its physical location is selected in software as one of CH1…CH6 so the controller can use that channel's DS18B20 as the AUX-zone temperature reference. AUX assistance is enabled only for COMFORT REACH events selected by the user.

The software also supports per-channel `outputInverted` for ON/OFF heater-style outputs. Default is normal (not inverted). This setting does not swap the two direction outputs of a motorized ball valve; valve OPEN/CLOSE remain explicit.

### Relay count planning

- CH1…CH6 with full two-relay valve capability: 12 relay outputs.
- AUX TEN: +1 relay output.
- Total for the universal 6-channel design: **13 relay outputs**.

Therefore, two 6-relay boards provide only 12 outputs. They are sufficient only if one output is otherwise unused. For a universal design where all six channels may be motorized valves and AUX is always present, use 6+8, 8+8, or 6+6 plus one separate relay.

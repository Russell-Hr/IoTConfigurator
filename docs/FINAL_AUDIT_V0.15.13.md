# HeatControl V0.15.13 HARDENED — 6CH FINAL audit

## 1. Hardware/software mapping

| Function | Assignment |
|---|---|
| 1-Wire | GPIO4 |
| DS3231 SDA/SCL | GPIO21 / GPIO22 |
| CH1 | GPIO16 / GPIO17 |
| CH2 | GPIO18 / GPIO19 |
| CH3 | GPIO23 / GPIO25 |
| CH4 | GPIO26 / GPIO27 |
| CH5 | GPIO32 / GPIO33 |
| CH6 | GPIO13 / GPIO14 |
| AUX TEN | GPIO5 |
| MCP23017 | I²C 0x20, optional |

The firmware arrays, configuration constants and authoritative `docs/WIRING_6CH.md` use this same map.

## 2. Sensor model

Seven DS18B20 devices share GPIO4. Six are assigned to CH1…CH6 by persistent ROM ID. The seventh is a dedicated OUTDOOR sensor and is never treated as CH7. The firmware rejects assigning the OUTDOOR ROM to a heating channel.

## 3. Channel functions

Each of six channels supports:

- AUTO with independent WORKING/HOLIDAY weekly templates.
- COMFORT CONST.
- ECONOM CONST.
- OFF.
- Runtime-only COMFORT/ECONOMY override while in AUTO.
- Valve or heater actuator selection.
- COMFORT/ECONOMY setpoints and hysteresis.
- Per-channel sensor ROM assignment.
- Heater output inversion.
- TIME or optional LIMIT_SWITCH valve feedback.

## 4. Valve safety

Motorized valves use A=OPEN and B=CLOSE, with both outputs OFF for STOP. Reversal has a 100 ms software dead-time. TIME-mode movement is refused while position is unknown unless the channel is in the explicit calibration workflow. A TIME calibration becomes position-authoritative only after both full OPEN and CLOSE travel measurements are completed. LIMIT_SWITCH mode requires a healthy MCP23017 and stops on expander loss or contradictory endpoint inputs.

## 5. AUX TEN

AUX is one controller-wide ON/OFF helper heater on GPIO5, not a seventh channel. It can reference one physical channel zone, assist a configured channel mask, enforce a maximum zone temperature and runtime, and use Post-REACH policy: OFF, configured minutes, or continue to COMFORT, with an independent maximum Post-REACH runtime. Calendar events have priority.

## 6. Persistence/API

Configuration uses transactional two-slot NVS storage with verification before committing the active slot. `CONFIG_VERSION` remains 17. Existing migration paths are retained. Authenticated APIs expose channel settings, calendars, RTC/system functions, sensor assignment, self-test, reach learning and AUX settings.

## 7. Verification

- Supplied host contract suite: 76/76 PASS.
- Static 6CH pin-map check: PASS.
- Firmware and Android source are synchronized to the six-channel model.
- Physical hardware test: NOT RUN.
- Real PlatformIO build: NOT RUN in this environment.
- Real Android Gradle/APK build: NOT RUN in this environment.

## 8. Safety boundary

This is control software, not certified electrical safety equipment. Relay logic, isolation, protective devices, load ratings and all 230 V wiring must be verified separately by a qualified electrician.

# HeatControl V0.14.2 HARDENED

## Changes

### OUTDOOR DS18B20
- Added a seventh DS18B20 on the existing GPIO4 1-Wire bus.
- CH1…CH6 retain their own ROM-bound sensors.
- The seventh sensor is stored separately as `outdoorSensorRom` and is not a seventh heating channel.
- Web UI: live outdoor temperature is shown at the top of the Dashboard; assignment is under System.
- Android: live outdoor temperature is shown on Dashboard; assignment is under System.
- Missing/unassigned outdoor sensor is reported as OFFLINE/`--`.

### Valve calibration
- `LIMIT_SWITCH` valves do not use time calibration.
- `TIME` valves with unknown position show **"Необхідна калібровка клапана"**.
- Calibration UI has initial position `Закрито` or `Відкрито`, plus `СТАРТ` and `СТОП`.
- Starting from `Закрито` measures OPEN travel time.
- Starting from `Відкрито` measures CLOSE travel time.
- The measured full-travel seconds are persisted as the corresponding valve time.
- During calibration, the old configured travel time cannot terminate the measurement early; only STOP or the 300 s safety timeout ends it.
- Manual TIME-mode OPEN/CLOSE is blocked while position is unknown.

### Firmware/API
- Config version bumped to 16 (historical V0.14.2 release).
- This historical release added migration from V0.14.1 to V0.14.2 while preserving existing settings.
- Added `/api/system/outdoor` for outdoor sensor assignment.
- Added `/api/valve/calibration` with `action=start|stop` and `initial=closed|open`.
- Kept `/api/valve/calibrate` as an alias for compatibility.
- Fixed the previous undefined `getCh()` compile blocker by adding strict channel parsing for valve API commands.

## Verification

- Host contract tests: **31 passed**.
- JavaScript syntax: PASS via Node syntax check of extracted script.
- C++ structural balance: PASS (braces and parentheses balanced).
- ZIP integrity: PASS.
- Real PlatformIO ESP32 compilation and real Android Gradle build remain unexecuted in this environment because the required toolchains/dependency caches are unavailable.

# HeatControl V0.18 — 6CH production rebuild

6-channel ESP32 heating controller with web UI and Android companion app.

## Included

- ESP32 DevKit V1 / ESP32-WROOM firmware.
- 7 × DS18B20 on GPIO4 with persistent 64-bit ROM mapping: 6 channel sensors + 1 dedicated OUTDOOR sensor.
- DS3231 RTC on GPIO21/22.
- 12 channel outputs for CH1…CH6 plus 1 dedicated AUX TEN relay output on GPIO5 (13 logical relay outputs total).
- Four selectable actuator types in Web UI: servo valve with limit switches, timed servo valve, TEN/valve, and inverted valve output.
- AUTO / COMFORT / ECONOMY / OFF.
- Independent comfort/economy temperatures and hysteresis.
- Two independent 24-hour templates per channel (WORKING/HOLIDAY), 30-minute transition grid, plus a 7-day WORKING/HOLIDAY selector.
- Selectable valve position feedback per channel: TIME estimation or optional OPEN/CLOSE limit switches via MCP23017.
- Sensor error forces temperature-dependent channels to MANUAL; after two stable DS18B20 readings, the previously active mode/AUTO inversion is restored unless the user changed mode during the error.
- Compact transactional NVS persistence with two slots and legacy migration.
- Web UI served from LittleFS.
- Android app for up to 10 devices, mDNS discovery and manual host entry.
- Authenticated self-test API.
- Compile-time bench test mode that forces all physical relay outputs OFF.

## V0.15.7 Adaptive REACH — corrected historical model

The controller learns actual COMFORT heating time per channel and uses indoor temperature difference plus outdoor-temperature bins when available. Statistics persist separately in NVS and can be reset from the Web/Android calendar UI.


## V0.15.7 corrections

- Historical similarity now converts every completed sample from total minutes to **minutes/°C** before weighting it.
- The current prediction therefore remains dimensionally consistent: `predicted minutes = temperature delta × minutes/°C`.
- The REST learning history is returned newest-first, including the derived minutes/°C value for each sample.
- Web and Android no longer reverse an already newest-first history.
- Added regression contract tests for the normalization and circular history ordering.

## V0.15.4 calendar UI hardening

- Calendar UI now makes the meaning of the selected time explicit for every slot: START (begin switching at this time) or REACH (reach target temperature by this time).
- The selected meaning is shown directly below the control in Web UI and Android UI.
- Existing API field `transition=start|reach` and `CONFIG_VERSION 17` remain unchanged.
- REACH activation latch is reset when the calendar date changes.

## V0.15.3 hardening

- Controller credentials stored by the Android app are encrypted with Android Keystore-backed AES/GCM for both username and password.
- Global firmware CORS is disabled.
- Manual TIME-mode valve movement is refused while position is unknown; use the explicit calibration workflow first.
- Persistent ESP32 configuration remains compatible with `CONFIG_VERSION 17`.

## Final hardening

V0.15.3 retains selectable calendar transition semantics (START vs REACH), while retaining the dedicated OUTDOOR DS18B20, manual START/STOP TIME-mode valve calibration, and optional per-channel valve feedback selection: TIME or OPEN/CLOSE limit switches. Existing V0.14.2 schedules migrate with all slots set to START, preserving previous behavior.

## Important safety note

This is control software, not a certified electrical safety controller. Verify the actual relay module's logic, isolation, ratings and protection before connecting any load. All 230 V installation work must be performed by a qualified electrician.

## Verification limitation

The archive was statically and contract-tested. A real PlatformIO ESP32 build/flash and Android Gradle APK build were not performed here because the required toolchains/dependency caches were unavailable.

See `docs/REVIEW_REPORT.md`, `docs/BUILD_STATUS.md` and `tests/test_contract.py`.

## Calendar templates (V0.17)

Each channel now has two independent weekly templates:

- **WORKING** — one 24-hour template with up to 6 transitions.
- **HOLIDAY** — one independent 24-hour template with up to 6 transitions.
- Each channel has one template logic setting: START or REACH. The selected logic is shared by both WORKING and HOLIDAY templates.
- START means the transition begins at the specified time. REACH means the system must already have reached the selected temperature by the specified time. REACH preheating applies to COMFORT transitions; ECONOMY transitions switch at the specified time because there is no active cooling.
- Every weekday has a `WORKING/HOLIDAY` indicator in the weekly calendar.
- In `AUTO`, the RTC weekday selects exactly one of the two templates.

V0.12 configurations are migrated automatically: each old day's schedule is copied into the template that was selected for that day, while the other template keeps its default schedule.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml` builds the ESP32 firmware with PlatformIO and the Android APK with Gradle, and runs tests.


## V0.15 hardening

- Config version 17 adds a persistent ESP32 AP password with Web UI and Android controls.
- Existing V0.14.2 configurations migrate automatically and preserve the AP password and dedicated OUTDOOR sensor setting; calendar slots default to START semantics. V0.14 configurations migrate automatically and receive the bench default AP password; change it before deployment.
- MCP23017 detection verifies readable configuration registers and is retried periodically after loss.
- Valve direction reversal uses a 100 ms software dead-time. Hardware interlocking remains recommended.
- Limit-switch command failures return explicit API errors instead of reporting false success.
- Android status parsing includes all valve feedback/limit fields.
- Android includes a small JVM unit-test suite for endpoint URL construction.
- The authoritative wiring document and SVG use GPIO4 for 1-Wire and GPIO21/22 for the shared I²C bus.


### V0.15.7 changes
- Adaptive REACH now keeps the last 12 completed heating samples per channel.
- Prediction uses similarity of initial indoor temperature, target temperature, and outdoor temperature when available.
- The long-term EWMA coefficient remains as a stability anchor.
- An unfinished sample is discarded if a later calendar event takes over or the deadline is missed.
- Learning starts only from a valid non-heating baseline: heater OFF, or valve stopped/known at 0%.
- Web and Android calendar screens show recent learning history.
- Learning storage remains separate from configuration NVS; `CONFIG_VERSION` stays 17.


## V0.15.8 corrections

This release hardens TIME-mode valve calibration and Adaptive REACH persistence. A TIME valve becomes position-known only after both full OPEN and CLOSE travel times have been measured. REACH-learning reset persistence now verifies the NVS write result and returns an API storage error when persistence fails.


## V0.15.9 — AUX TEN + output inversion
- Added one controller-wide AUX TEN output on GPIO5; AUX is separate from CH1–CH6 and is used only for REACH assistance.
- AUX can be assigned a physical location channel, a set of channels it may assist, maximum runtime, and maximum temperature of its own zone.
- Added per-channel ON/OFF output inversion; default is normal/non-inverted. Motorized two-relay valves keep fixed OPEN/CLOSE direction mapping.
- Existing V0.15.8 configuration remains compatible; missing AUX/inversion keys load as safe defaults.


## V0.15.10 hardening

- AUX is strictly pre-deadline assistance; it stops being requested once the active REACH deadline arrives.
- Protective AUX OFF is immediate and bypasses the anti-chatter delay.
- Per-channel output inversion is included in transactional NVS slot verification.
- GPIO5 is explicitly documented as an ESP32 strapping pin.
- Wiring diagram and release documentation are aligned with the 6CH + AUX architecture.

Build note: host-side tests and static checks pass; a real PlatformIO ESP32 build and Android Gradle/APK build still require the corresponding toolchain environment.


## AUX Post-REACH
V0.15.11 adds programmable behavior when COMFORT REACH is still not achieved at its deadline. AUX can be disabled after the deadline, continue for a configured number of minutes, or continue until COMFORT, subject to an independent Post-REACH maximum runtime and the AUX zone temperature limit. The next calendar event always has priority.


## V0.18.2
Firmware includes the nonlinear normalized round-port ball-valve flow characteristic requested for the 6-channel controller. The approved Web UI is unchanged.

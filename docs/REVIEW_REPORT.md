# HeatControl V0.14.1 HARDENED — comprehensive release audit

## Scope

Final hardening of the 6-channel ESP32-WROOM/DevKit V1 controller and Android companion app.

## Hardware/software contract

- 6 independent channels.
- 6 DS18B20 sensors on one 1-Wire bus, GPIO4.
- DS3231 on I²C GPIO21/GPIO22.
- 12 relay outputs: two per channel.
- CH1 GPIO16/17; CH2 18/19; CH3 23/25; CH4 26/27; CH5 32/33; CH6 13/14.
- Valve mode uses A=OPEN and B=CLOSE; both OFF means STOP.
- Heater mode uses A as the heater output and forces B OFF.

## V0.14.1 hardening

1. **Transactional NVS slots.** Configuration is written to the inactive slot, read back and byte-verified, then the active-slot selector is committed last. A power interruption before selector commit leaves the previous valid slot active.
2. **Dual-slot recovery.** Startup first tries the selected slot and then the other slot. Invalid/incomplete slots are rejected by version, blob-size and configuration validation.
3. **V0.11 migration.** The previous compact `heatcfg` namespace is imported only after validation and successful transactional save; it is then cleared. Legacy key-by-key storage is cleared only after successful migration.
4. **Bench test mode.** `HEATCONTROL_TEST_MODE=1` forces physical relay outputs OFF while retaining control/state logic for software/bench tests. Default release value is 0.
5. **Self-test endpoint.** Authenticated `GET /api/system/selftest` reports storage, RTC, LittleFS, STA connectivity, sensor count, configuration validity and whether physical outputs are forced OFF by test mode.
6. **IPv6-safe Android URLs.** IPv6 literals are automatically bracketed when constructing HTTP URLs.
7. **Modern Android NSD.** API 34+ uses `ServiceInfoCallback`; older Android versions retain the legacy per-service `resolveService()` fallback.
8. **Documentation synchronized to V0.14.1.**

## Safety behavior

- Heater sensor failure forces physical OFF.
- Heater OFF/safety recovery cannot immediately re-enable output; the switching interval is respected.
- Valve OPEN and CLOSE are mutually exclusive in firmware.
- Unknown valve position after reboot remains stopped until calibration/known state is established.
- Physical outputs start OFF.

## Known limitations

- Relay active HIGH/LOW must be verified against the actual relay module.
- In `TIME` mode, valve position is estimated from movement time. In `LIMIT_SWITCH` mode, OPEN/CLOSE endpoints are read from the optional GPIO expander; there is still no continuous analog position sensor.
- HTTP Basic Auth is not HTTPS.
- AP password and default credentials must be changed before deployment.
- Real ESP32 and Android builds were not available in this execution environment.
- 230 V wiring and protection require qualified electrical verification.

## V0.14.1 additional hardening

9. Config version raised to 15 and V0.14 migration added for the new persistent AP password.
10. ESP32 AP password is no longer hard-coded at runtime; it is stored in transactional NVS and can be changed through authenticated Web UI or Android.
11. MCP23017 probing now verifies IODIR and GPPU readback and retries detection periodically.
12. Valve reversal dead-time increased from 5 ms to the configured 100 ms software interval.
13. LIMIT_SWITCH command errors are returned explicitly; the controller does not silently claim success when the expander is unavailable or a valve fault is present.
14. Android channel parsing was corrected to populate valve feedback and limit-switch fields that already exist in the data model/UI.
15. Added Android JVM unit tests and removed release-cache artifacts.
16. Corrected the authoritative wiring documentation to the actual GPIO4 / GPIO21 / GPIO22 map and MCP23017 I²C topology.

## Verdict

**V0.14.1 HARDENED is the final software hardening package for bench integration.** It is not an electrical certification and does not claim a successful hardware flash/APK build without the corresponding toolchains and hardware.

## V0.14.1 calendar correction

The calendar model was changed from one schedule per day with a stored day type to two independent weekly templates plus a day-type selector. Each channel now stores `WORKING[7][6]`, `HOLIDAY[7][6]`, and `dayType[7]`. AUTO selects the template from the RTC weekday's `dayType` and executes only that template's six transitions. The web API and UI expose both templates separately. V0.12 binary configurations are migrated automatically.

## V0.14.1 implementation note

V0.14.1 adds per-channel valve position feedback selection: `TIME` or `LIMIT_SWITCH`. The optional MCP23017 at I²C address `0x20` supplies 12 endpoint inputs for six valves. Absence of the expander does not disable the controller; it only makes limit-switch mode unavailable. Existing V0.13 configurations migrate to V0.14.1 with `TIME` selected for every channel.

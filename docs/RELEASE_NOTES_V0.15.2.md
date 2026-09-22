# HeatControl V0.15.2 HARDENED RC

## Changes

- Firmware `saveConfiguration()` is defined before `stopCalibration()`; storage helper declarations are explicit.
- `saveConfiguration()` no longer blocks configuration writes because of sensor assignment diagnostics.
- Sensor-assignment health is now exposed separately by `/api/system/selftest` as `sensorAssignments`; it does not invalidate otherwise structurally valid configuration.
- Android mDNS uses the deprecated-compatible `NsdManager.resolveService()` path on all supported versions; API 34 `ServiceInfoCallback` code was removed.
- REACH scheduling now uses a per-day activation latch (`reachActivatedMask`) so an activated REACH transition does not move when temperature readings fluctuate.
- Channel/calendar writes invalidate the REACH cache/latch so changing a schedule or setpoint during the same day cannot reuse stale timing.
- REACH + ECONOMY remains an early-start semantic because heating-only hardware cannot guarantee cooling to the economy target.
- Existing 64 KiB NVS partition is retained for transactional two-slot configuration storage.
- Test suite updated to enforce the stable mDNS contract and the new REACH/save semantics.

## Verification

- Python contract tests: 45/45 PASS.
- Firmware structural brace/parenthesis check: PASS.
- Web UI JavaScript syntax check with Node: PASS.
- `ServiceInfoCallback` references: 0.
- Generated package excludes Python cache artifacts.

A real PlatformIO firmware build and Android Gradle build still require the corresponding external toolchains and are not claimed as executed here.

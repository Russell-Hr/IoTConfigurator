# HeatControl V0.15.7 HARDENED — Adaptive REACH correction

## Critical correction

V0.15.6 stored actual heating duration in minutes. The similarity predictor must compare heating efficiency in minutes per degree, not raw duration.

V0.15.7 normalizes every historical sample as:

`minutesPerDegree = actualMinutes / (targetTemperature - initialIndoorTemperature)`

Only positive temperature rises greater than 0.25 °C and coefficients in the safe 1–30 min/°C range participate in similarity weighting. The weighted result is therefore a coefficient in min/°C, and the final lead calculation multiplies it by the current temperature delta exactly once.

## History ordering

The firmware REST API now exposes the circular 12-sample history newest-first. Each returned sample includes `minutesPerDegree`; Web UI and Android consume that order directly.

## Regression coverage

- 61/61 Python contract tests pass.
- Added explicit regression tests proving raw `minutes` are not used as a min/°C coefficient.
- Added history-order and API-field tests.
- Web JavaScript syntax check passes.

## Compatibility

`CONFIG_VERSION` remains 17. The `reachlearn` NVS structure is unchanged from V0.15.6, so existing learned samples remain usable; their normalized min/°C value is calculated when the model reads them.

## Build limitation

A real PlatformIO firmware build and full Android Gradle build still require the external toolchains/dependency caches unavailable in the audit environment.


## V0.15.8 hardening corrections

- TIME-mode valve position is now considered authoritative only after both OPEN and CLOSE travel measurements are completed. A one-direction calibration no longer marks the valve position as known.
- Adaptive REACH learning persistence now verifies the number of bytes written to NVS and the reset API reports storage failure instead of unconditional success.
- Web calibration guidance now explicitly requires measuring both travel directions.

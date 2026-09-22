# HeatControl V0.18 production-UI integration build status

- Approved 6-channel UI integrated into `firmware/data/index.html`.
- Dashboard structure preserved; channel settings and group settings use the approved template/weekly-schedule UI.
- Weekly schedule: two horizontal rows (working/holiday), seven day indicators; each weekday belongs to exactly one row.
- Template logic: `Почати` / `Досягти`; one persistent setting per channel is shared by both WORKING and HOLIDAY templates. REACH preheats COMFORT transitions; ECONOMY transitions switch at the configured time.
- Production UI uses the ESP32 REST API (no demo/mock data).
- Sensor-loss safety: a detected channel sensor failure latches the channel into MANUAL and switches outputs off; after two stable DS18B20 readings, the previous mode/AUTO inversion is restored unless the user changed mode during the error.
- Adaptive REACH historical samples are normalized as minutes per degree before similarity weighting.

## Verification

- Python contract tests: 84 passed with `python -m pytest -q tests/test_contract.py`.
- Web UI JavaScript syntax: extracted script checked with Node.js `--check`.
- ESP32 native PlatformIO build was not available in this environment because `pio` is not installed; this is explicitly not claimed as a hardware build.


## V0.18 logic update — 2026-09-18
- Replaced per-slot START/REACH semantics with one channel-level template logic setting.
- Configuration version 18 migrates V0.17 settings with START as the default.

## V0.16 production rebuild — 2026-09-18
- Rebuilt from the approved 6-channel project archive.
- Added automatic DS18B20 sensor-error recovery: remember previous mode/AUTO inversion, force MANUAL on loss, restore after two stable readings unless the user changes mode during the error.
- Preserved approved Dashboard markup and styling.
- Wiring reference regenerated from the firmware GPIO map; EXT GPIO is MCP23017 @ 0x20 for 12 limit-switch inputs.

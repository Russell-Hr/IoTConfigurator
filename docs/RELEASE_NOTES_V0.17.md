# HeatControl V0.18 — production rebuild

## Basis
Built from `HeatControl_V0.16_APPROVED_UI_6CH.zip` with the approved Web UI preserved.

## Firmware
- 6 independent heating channels.
- 7 DS18B20 sensors on the shared GPIO4 1-Wire bus: CH1–CH6 + OUTDOOR.
- DS3231 on GPIO21/22.
- EXT GPIO is MCP23017 at I²C address `0x20`, used for 12 OPEN/CLOSE limit-switch inputs.
- 12 channel relay outputs: two per channel.
- AUX TEN relay on GPIO5; AUX is not CH7.
- Four actuator selections are represented consistently: limit-switch servo, timed servo, normal TEN/valve, inverted TEN/valve output.
- Adaptive REACH historical samples are normalized to minutes/°C before similarity weighting.
- DS18B20 sensor-loss recovery now remembers the previous temperature-controlled mode and AUTO inversion, forces MANUAL while the sensor is unavailable, and restores the previous mode after two stable readings unless the user changed mode during the error.

## Web UI
- Approved Dashboard structure and styling preserved.
- Channel settings: two 24-hour templates, 30-minute transitions, one per-channel START/REACH logic setting shared by WORKING and HOLIDAY, weekly WORKING/HOLIDAY two-row indicators.
- Group settings: copy from a source channel, group parameters, templates, weekly schedule, channel selection and Apply/Cancel/Back actions.
- Channel settings bottom bar: Back → Cancel → Save.
- Calendar is no longer a sidebar destination; group settings are used for multi-channel schedule/parameter changes.
- Dashboard channel settings navigation uses the corresponding sidebar channel button rather than the Dashboard card button as the active navigation element.

## Verification performed
- 84 Python contract tests passed.
- Web UI JavaScript syntax checked with Node.js `--check`.
- No duplicate HTML IDs.
- Wiring SVG XML validated.
- Old pytest cache/bytecode removed from the release archive.

## Build limitation
PlatformIO is not installed in the current execution environment, so no physical ESP32 firmware compile/flash is claimed.

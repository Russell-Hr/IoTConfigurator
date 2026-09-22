# HeatControl V0.18.3 — Servo operation modes

## New
When a channel uses a servo actuator, channel settings now include two mutually exclusive operation modes:

- **Регулятор** — intermediate mechanical positions are allowed; the existing timed-position control can stop at intermediate angles.
- **Клапан** — binary operation only: fully **ЗАКРИТО** or fully **ВІДКРИТО**. Manual intermediate hold controls are hidden/disabled for this mode.

## Firmware/API
- Added persistent `servoMode` per channel (`regulator` / `valve`).
- Existing configurations default to `regulator` when the new key is absent.
- `GET /api/channels` now reports `servoMode`.
- `PUT /api/channels/{id}` accepts `servoMode`.
- Configuration storage remains backward-compatible because the mode is stored separately from `ChannelConfig`.
- Existing valve position persistence and nonlinear ball-valve flow model remain unchanged.

## UI
- Approved channel settings layout preserved.
- Added only the requested two-button selector under the servo actuator settings.
- Mock UI updated to demonstrate the same selector and conditional manual controls.

## Verification
- Python contract tests: 87/87 PASS.
- Firmware source was not compiled with PlatformIO in this environment because the ESP32 toolchain is unavailable here.

# HeatControl V0.18.4 — Servo mode scope fix

## Change
The `Регулятор / Клапан` servo operation mode now applies to **automatic operation only**.

### Manual mode
Manual servo controls are identical for both servo operation modes:
- `OFF`
- `◀ Закривати` — hold while moving toward closed
- `Відкривати ▶` — hold while moving toward open
- `OPEN`

Selecting `Клапан` no longer hides the two intermediate manual movement buttons.

### Automatic mode
The selected servo operation mode remains available as the automatic-operation mode setting:
- `Регулятор` — automatic control may use intermediate angular positions.
- `Клапан` — automatic control is endpoint-only: fully closed / fully open.

## UI
No approved dashboard structure or visual design was changed. Only the incorrect conditional hiding of manual intermediate controls was removed.

## Verification
- Contract tests: 87/87 PASS
- Firmware Web UI JavaScript syntax: PASS
- Mock UI JavaScript syntax: PASS
- ESP32/PlatformIO compilation: not run; ESP32 toolchain is unavailable in this environment.

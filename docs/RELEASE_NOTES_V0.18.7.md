# HeatControl V0.18.7 — HARDENED RELEASE CANDIDATE

## Зміни після супер-аудиту V0.18.5

### 1. AUTO: Регулятор / Клапан
- `Клапан` в AUTO працює тільки по крайніх положеннях 0% / 100%.
- `Регулятор` в AUTO використовує проміжну цільову позицію за температурною похибкою.
- Додано внутрішню пропорційну шкалу регулятора без зміни approved UI.
- Для обох режимів збережені існуючі manual-керування та safety gates.

### 2. Servo preventive maintenance
- NVS schema піднята до version 2.
- Під час процедури зберігаються активність, фаза, канал і позиція повернення.
- Після reboot процедура НЕ продовжується сліпо.
- Якщо процедура була перервана живленням, позиція клапана позначається невідомою до повторної валідації.
- Для клапанів з кінцевиками фізичні крайні положення після reboot знову визначаються кінцевиками.
- Черга залишається послідовною: один клапан за раз, між клапанами рівно 180 секунд.

### 3. MCP23017 recovery
- Після відновлення MCP23017 fault більше не знімається миттєво.
- Спочатку виконується повторне читання та debounce входів.
- Лише валідний стан кінцевиків дозволяє зняти `limitHardwareFault`.

### 4. UI
- Approved production UI не перероблявся.
- Додані firmware-only зміни не змінюють структуру Dashboard / Settings.

## Перевірки
- Python contract tests: **91 passed**.
- Production Web UI JavaScript syntax: **PASS**.
- C++ brace-balance / duplicate-function static checks: **PASS**.
- Повноцінний PlatformIO ESP32 build у цьому середовищі: **не виконаний**, тому цей пункт залишається для GitHub CI / PlatformIO environment.

## Важливо
Ця версія закриває програмні дефекти, знайдені під час аудиту V0.18.5. Фізична електрична перевірка MCP23017-модуля, рівнів I²C, реле, кінцевиків та 230 V силової частини все одно має виконуватися окремо.


## V0.18.7 hardening changes

- AUTO `Клапан` mode is endpoint-only: the controller always drives toward 0% or 100% and never intentionally stops at an intermediate position.
- Preventive-maintenance NVS format bumped to version 3.
- Maintenance reboot handling remembers the interrupted channel and invalidates only that valve position.
- Preventive maintenance is never blindly resumed after reboot; outputs remain under normal safe-start/revalidation rules.
- Added behavioral contract tests for endpoint-only valve mode and maintenance reboot isolation.
- Release archive is cleaned of pytest caches, `__pycache__`, and `.pyc` artifacts.
- Wiring document version updated to V0.18.7.

## Verification

- Python contract tests: 96/96 PASS.
- Python compileall: PASS.
- Firmware brace/parenthesis balance: PASS.
- Production Web UI source was not redesigned or modified.
- Native PlatformIO ESP32 build remains a required CI/hardware gate and is not claimed as locally executed in this environment.

# HeatControl V0.18.5

## Servo-valve preventive maintenance

Added a separate sidebar section **Профілактика** immediately after **Групові налаштування**.

### User-facing settings
- Enabled by default.
- Default time: **03:00**.
- Frequency is fixed at **1 time per week**; it is not configurable.
- Queue pause is fixed at **3 minutes**; it is not configurable.

### Eligibility
Maintenance is performed only for channels that are:
- Servo actuator.
- Servo operation mode: **Регулятор**.
- Channel mode: **AUTO**.
- Valve position is known and there is no valve fault/calibration in progress.

It is not performed for **Клапан**, Manual, Comfort constant, Economy constant, or OFF modes.

### Cycle
For every eligible channel, sequentially:
1. Save the current valve position.
2. Full close to 0%.
3. Full open to 100%.
4. Return to the saved position.
5. Persist the restored position.
6. Wait 3 minutes before starting the next valve.

Only one preventive-maintenance valve may move at a time.

The procedure is not delayed merely because the channel is actively heating; the heating system is treated as inertial and the approximately one-minute maintenance cycle is allowed to run.

### UI
No maintenance controls were added to channel cards. The approved channel UI remains unchanged apart from the previously requested servo mode behavior.

## Validation
- Contract tests: **87/87 PASS**.
- Web UI JavaScript syntax: PASS.
- Mock UI JavaScript syntax: PASS.
- ESP32 PlatformIO compilation: not performed because the ESP32 toolchain is unavailable in this environment.

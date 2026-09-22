# HeatControl V0.15.11 — Post-REACH audit

## Change audited
AUX TEN can now be programmed to continue after a missed COMFORT REACH deadline.

## Logic
- Before deadline: Adaptive REACH predictive assistance is unchanged.
- At/after deadline: Post-REACH policy may keep AUX active while the same COMFORT REACH event remains active.
- OFF policy disables post-deadline assistance.
- MINUTES policy stops after `postReachMinutes`.
- COMFORT policy continues until COMFORT is reached.
- `postReachMaxSeconds` is an independent post-deadline safety limit.
- A later calendar event stops assistance because `activeScheduleEvent()` changes.

## Persistence/API/UI
- New optional NVS keys: `auxPostPol`, `auxPostMin`, `auxPostMax`.
- Transactional slot verification covers all three values.
- Web UI and Android expose all three settings.
- `CONFIG_VERSION` remains 17 for backward compatibility.

## Verification
- Python contract tests: 74/74 PASS.
- Python syntax/compile: PASS.
- Web UI: contract coverage PASS; JavaScript syntax should be checked with the project's normal extraction step.
- Real PlatformIO firmware build: NOT RUN in this environment.
- Real Android Gradle/APK build: NOT RUN in this environment.
- Physical hardware test: NOT RUN.

## Example
At 18:00 REACH is missed and the room is 18.0 °C while COMFORT is 21.0 °C. With Post-REACH=`COMFORT`, AUX may continue after 18:00 and stop when COMFORT is reached or a safety/calendar limit takes precedence.

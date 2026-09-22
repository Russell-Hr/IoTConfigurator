# HeatControl V0.15.10 — final host-side audit

## Verified

- 6 independent heating channels.
- 7 DS18B20 sensors on one 1-Wire bus: CH1…CH6 + OUTDOOR.
- DS3231 RTC.
- 12 channel relay outputs plus one controller-wide AUX TEN logical output on GPIO5.
- AUX is not CH7 and is used only as COMFORT REACH assistance.
- AUX deadline guard: after the active REACH deadline, the event cannot request AUX.
- Protective AUX OFF bypasses the anti-chatter delay.
- Per-channel ON/OFF output inversion is persisted and transactionally verified.
- Motorized valve OPEN/CLOSE direction remains separate from heater inversion.
- TIME valve position becomes known only after both calibration directions.
- Optional MCP23017 limit-switch architecture remains intact.
- Adaptive REACH historical model remains intact after the V0.15.7 mathematical correction.
- Transactional two-slot NVS architecture remains intact.
- Updated 6CH + AUX wiring reference, SVG and PNG.

## Host-side results

- Contract tests: 73/73 PASS.
- Python syntax/compile check: PASS.
- Web JavaScript syntax check: PASS.
- C++ brace balance: 545/545.
- C++ parenthesis balance: 2265/2265.
- SVG export to PNG: PASS.
- ZIP integrity: verified after packaging.

## Not claimed

A real PlatformIO ESP32 compilation and a real Android Gradle/APK build were not available in the current environment. The archive must not be described as hardware-build-verified until those builds are executed.

## Hardware note

GPIO5 is an ESP32 strapping pin. Espressif documents GPIO5 as a strapping pin sampled during startup and then usable as a normal GPIO; the selected relay input circuit must preserve the required boot-time strap level.

230 V power wiring, protection, relay contact ratings and installation are outside this logical controller diagram and require qualified electrical verification.

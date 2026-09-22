# HeatControl CI

The repository contains a GitHub Actions workflow for reproducible cloud verification.

## Firmware

- Installs PlatformIO.
- Builds the ESP32 Arduino firmware with `pio run`.
- Runs host-side contract tests.

## Android

- Installs JDK 17 and Android SDK 34.
- Installs Gradle 8.4.
- Runs `testDebugUnitTest` and `assembleDebug`.

The CI workflow is intended to provide the real compiler/build checks that are not available in the local sandbox used to prepare this archive.

It does not authorize or simulate connection to 230 V equipment. Hardware commissioning and electrical safety remain separate activities.

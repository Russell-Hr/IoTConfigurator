# HeatControl V0.15.6 HARDENED — richer Adaptive REACH

## What changed

### 1. Historical model
Each channel now stores up to 12 completed REACH samples containing:
- initial indoor temperature;
- comfort target temperature;
- outdoor temperature when available;
- actual elapsed heating time.

The REACH estimator first looks for historically similar cases and uses their measured minutes-per-degree as the prediction. Similarity considers indoor start temperature, target temperature, and outdoor temperature. The long-term EWMA remains as a small stability anchor.

### 2. Learning-data quality
A new sample is started only when the controller has a clean baseline:
- TEN/heater: output is OFF;
- valve: position is known, stopped, and at 0%.

This avoids measuring a cycle that was already partly heated/open before the REACH event.

### 3. Session cancellation
An active learning session is discarded without updating statistics when:
- the calendar date changes;
- another calendar event replaces the original REACH event;
- the deadline passes before the comfort target is reached.

This prevents late or unrelated heating from contaminating the historical model.

### 4. UI
The Web UI and Android calendar screen now expose recent historical samples so the user can see what the controller has actually learned.

### Compatibility
Adaptive REACH is stored in the separate `reachlearn` NVS namespace. `CONFIG_VERSION` remains 17, so normal channel/calendar configuration is not invalidated by this release. Existing V0.15.5 learning records use a different stored structure and are safely reinitialized.

## Verification
- Contract/static tests: **57/57 PASS**.
- ZIP integrity: verified with `unzip -t`.
- A real PlatformIO firmware build was not run because PlatformIO is not installed in the current environment.
- A full Android Gradle build was not run in the current environment.

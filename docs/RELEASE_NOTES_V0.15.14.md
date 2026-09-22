# HeatControl V0.15.14 — approved dashboard + control logic

## UI
- Integrated the approved 6-channel dashboard UI into `firmware/data/index.html`.
- Approved card layout, styling, timeline, template controls, and inactive grayscale behavior are preserved.
- Outside AUTO: calendar label/timeline/template buttons are informational and disabled.
- In AUTO: calendar template buttons are interactive.
- Dashboard time/date and current-time marker are live values.

## Channel modes
- Added persistent `MANUAL` channel mode (`MODE_MANUAL=4`) while retaining compatibility with existing modes.
- Modes: MANUAL, ECONOMY CONST, COMFORT CONST, AUTO.
- MANUAL heater ON/OFF is controlled through the channel API and reflected immediately in the dashboard.
- MANUAL valve commands remain available through the existing valve API.

## AUTO inversion
- First press from a non-AUTO mode enters AUTO and clears inversion.
- Second press on AUTO enables inversion.
- Third press disables inversion.
- Subsequent presses repeat the inversion toggle.
- `autoMode` reports the scheduled mode; `effectiveMode` reports the mode after inversion.

## Calendar
- Weekly calendar remains per-channel.
- Dashboard loads the real per-channel calendar data and uses today's scheduled day type.
- Template preview in AUTO is UI-only and does not modify the weekly calendar.

## Android API model
- Added `autoMode` and `autoInverted` to `ChannelInfo` parsing.
- Added MANUAL to the channel-detail mode list without changing the existing Android visual design.

## Validation
- Web UI JavaScript: `node --check` passed.
- Contract tests: **80 passed**.
- PlatformIO executable was not available in the build environment, so a real ESP32 compile was not claimed.

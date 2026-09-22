# HeatControl V0.15.4 HARDENED

## Calendar transition UX

Each calendar slot now presents an explicit choice for what the user-selected time means:

- **Почати переключення о цьому часі** — the transition starts at the selected time.
- **Досягти температури до цього часу** — the selected time is the target/deadline; REACH calculates an earlier start.

The Web UI displays the selected meaning under every slot. The Android calendar screen uses two visible chips and a short explanation of the active choice.

The API remains backward-compatible with `transition=start|reach`, and persistent `CONFIG_VERSION 17` is unchanged. Existing schedules therefore keep their stored semantics.

## Runtime correction

The daily REACH activation latch is now cleared together with the daily REACH cache when the RTC calendar date changes. This prevents a slot activated on a previous day from being treated as already activated on the next day.

## Verification

Static contract tests cover the explicit Web/Android transition controls and the daily latch reset. Real PlatformIO/Android builds still require the project's CI or a local build environment.

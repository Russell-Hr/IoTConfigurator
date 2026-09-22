# HeatControl API — 6-channel

Base URL: `http://<device>/`

Authentication: HTTP Basic Auth for the web UI and `/api/*`, except `/api/discover`.

## Discovery

`GET /api/discover`

## Status

`GET /api/status`

Returns all 6 channels and the discovered DS18B20 list.

## Channels

`GET /api/channels`

`PUT /api/channels/{1..6}`

Fields: `name`, `actuator`, `mode`, `sensorIndex`, `sensorRom`, `comfort`, `economy`, `hysteresis`, `openTime`, `closeTime`, `valveFeedback`. A currently disconnected sensor may remain assigned by its persistent ROM while unrelated channel settings are edited.

Actuator values:

- `valve`
- `heater`

Mode values:

- `auto`
- `comfort`
- `economy`
- `off`

## Calendars

`GET /api/calendar/{1..6}`

`PUT /api/calendar/{1..6}`

Each channel has 7 days × 6 slots. Every day stores its own `working`/`holiday` type and every slot can switch between `comfort` and `economy`.

## Valve commands

`POST /api/valve/open?ch={1..6}`

`POST /api/valve/close?ch={1..6}`

`POST /api/valve/stop?ch={1..6}`

`POST /api/valve/calibrate?ch={1..6}`

Commands are accepted only when the selected channel is configured as `valve`.

## RTC

`GET /api/rtc`

`PUT /api/rtc`

`POST /api/rtc/ntp`

## System

`POST /api/system/reboot`

`POST /api/system/reset`

`PUT /api/system/auth`

`PUT /api/system/wifi`

`PUT /api/system/ap` — change the controller's own AP password; authenticated; 8–63 characters; reboot required

## V0.15 valve feedback

Each channel exposes `valveFeedback` as `time` or `limits`. Status additionally reports `limitSwitchAvailable`, `openLimit`, `closeLimit`, and `valveFault`. A channel may select `limits` only when the optional GPIO expander is detected.


## V0.15 calendar slot transition
Each schedule slot contains `transition`: `start` or `reach`. `start` begins the selected COMFORT/ECONOMY transition at the specified clock time. `reach` treats the specified clock time as a deadline; firmware starts the transition earlier using an initial 10-minutes-per-degree heuristic, bounded to 5–120 minutes. A REACH slot is latched once its predicted start is reached, so later temperature fluctuations cannot move it backward. REACH+ECONOMY is treated as an early-start request because heating-only hardware cannot guarantee cooling. Channel/calendar changes invalidate the runtime REACH cache.

`PUT /api/system/outdoor` accepts a 16-character DS18B20 ROM to assign the dedicated OUTDOOR sensor, or an empty `sensorRom` string to clear the assignment.

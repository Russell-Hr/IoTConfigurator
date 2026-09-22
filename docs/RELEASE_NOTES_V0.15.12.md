# HeatControl V0.15.12 — Channel manual modes

## Channel operating modes

1. **OFF** — valve fully closes / heating output off; remains until the user changes the base mode.
2. **ECONOM CONST** — permanent economy target; weekly calendar is ignored.
3. **COMFORT CONST** — permanent comfort target; weekly calendar is ignored.
4. **AUTO** — follows the channel weekly calendar.

### One-shot manual override in AUTO

When a channel is in AUTO, the UI can temporarily select COMFORT or ECONOMY. This is a runtime-only override. It automatically clears when the next calendar event becomes effective, when the date changes, or when the base mode is changed. The override does not modify the saved calendar or persistent base mode.

API: authenticated `POST /api/channels/{id}/override` with `{\"mode\":\"comfort\"}`, `{\"mode\":\"economy\"}`, or `{\"mode\":\"clear\"}`.

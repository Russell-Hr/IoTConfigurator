# HeatControl V0.15

## Calendar transition semantics

Each enabled calendar slot now has a transition type:
- `start` — start the transition at the specified time.
- `reach` — the specified time is the desired deadline by which the selected target temperature should already be reached.

For `reach`, firmware estimates a lead time from the current temperature difference: 10 minutes per °C, bounded to 5–120 minutes; if the channel sensor is unavailable it uses a conservative 60-minute fallback. This is an initial deterministic heuristic and is intentionally a foundation for later adaptive learning.

Existing V0.14.2 schedules migrate to `start`, preserving prior behavior.


## Compatibility
- Configuration version: 17.
- V0.14.2 calendar slots are migrated to `transition=start`, so existing schedules keep their old behavior.
- Older V0.13/V0.14/V0.15 storage migrations remain supported through the existing migration chain.

## Important implementation note
`reach` currently uses a deterministic initial heuristic (10 minutes per degree of temperature difference, bounded to 5–120 minutes; 60-minute fallback if the channel sensor is unavailable). This is deliberately a first-stage implementation. Adaptive learning using historical heat-up/cool-down behavior and outdoor temperature can be added without changing the calendar API.

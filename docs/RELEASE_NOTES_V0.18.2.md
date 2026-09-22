# HeatControl V0.18.2 — nonlinear ball-valve flow characteristic

## Change
Added a firmware-only normalized flow-capacity model for the typical round-port motorized ball valve used by the project. The approved Web UI is unchanged.

## Characteristic
The control model follows the requested 0–90° description:

| Valve angle | Relative flow capacity |
|---:|---:|
| 0° | 0% |
| 10° | 0% |
| 20° | 2% |
| 30° | 10% |
| 40° | 30% |
| 50° | 55% |
| 60° | 72% |
| 70° | 85% |
| 80° | 96% |
| 90° | 100% |

The firmware converts the stored mechanical position (0–100%) to 0–90° and linearly interpolates between the characteristic points.

## Important distinction
`valvePosition` remains the mechanical travel position. `valveFlowPercent` is a normalized relative flow-capacity estimate from the model; it is not a measured instantaneous water flow rate.

## UI
No visual/UI changes were made. Existing dashboard/settings layout and controls are preserved.

## Verification
Static contract tests cover the characteristic table, angle conversion, interpolation function presence, and API exposure. PlatformIO hardware compilation remains environment-dependent and must be performed with the ESP32 toolchain.

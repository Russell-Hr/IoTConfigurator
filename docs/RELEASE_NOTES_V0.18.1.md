# HeatControl V0.18.1 — persistent valve position

## Change
The last known valve position is now stored in a dedicated NVS namespace (`valvepos`) for every valve channel.

## Behavior
- A normal ESP32 reboot restores the last saved `valvePosition` (0–100%) and `positionKnown` state.
- The position is saved when a valve finishes a movement and its position is known.
- Long movements are periodically checkpointed every 10 seconds to limit position loss after an unexpected power interruption.
- Idle control cycles do not write the position repeatedly, avoiding unnecessary flash wear.
- The existing transactional `ChannelConfig` storage is unchanged.
- Limit-switch endpoints still establish authoritative 0% / 100% positions when the corresponding end switch is reached.
- The approved UI is unchanged.

## Verification
- 86/86 contract tests pass.
- PlatformIO build could not be executed in this environment because `pio` is not installed.

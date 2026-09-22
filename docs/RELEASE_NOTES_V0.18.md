# HeatControl V0.18 — unified channel template logic

- Added one persistent template-logic setting per channel: START or REACH.
- The selected logic applies to both WORKING and HOLIDAY templates of that channel.
- START: a transition begins at the configured time.
- REACH: for COMFORT transitions, Adaptive REACH preheating is calculated so the selected comfort temperature is reached by the configured time.
- ECONOMY transitions remain direct time switches because the controller has no active cooling mechanism.
- Per-slot transition choices are no longer authoritative; the API exposes the channel-level `logic` and `templateLogic` values.
- Configuration version 18 migrates V0.17 configurations with START as the initial channel logic.
- Approved Dashboard UI remains unchanged.

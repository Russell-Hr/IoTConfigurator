# HeatControl V0.15.10

- Added one controller-wide AUX TEN output on GPIO5. AUX is not CH7.
- AUX has configurable physical location channel, eligible assisted channels, maximum runtime and maximum location temperature.
- AUX operates only as COMFORT REACH assistance when the Adaptive REACH estimate predicts a missed deadline.
- Added per-channel ON/OFF output inversion. Default: OFF. Motorized two-relay valve direction remains explicit OPEN/CLOSE.
- Existing V0.15.8 NVS remains compatible; absent new keys default safely.

Hardware planning: six channels with two relay outputs each need 12 relay outputs; AUX adds one. Therefore two 6-relay boards are one output short if every channel may be a valve. Use 6+8, 8+8, or 6+6 plus a separate single relay. Verify the exact relay module logic/isolation/contact ratings before any mains installation; 230 V work should be done by a qualified electrician.


## V0.15.10 HARDENED corrections

- AUX REACH request is now strictly pre-deadline: once the active REACH deadline arrives, that event cannot request AUX heat.
- Protective AUX OFF bypasses the 30 s anti-chatter delay; this applies to disabled AUX, missing AUX-zone sensor, AUX over-temperature, no longer-needed REACH, maximum runtime, and exhausted-run-limit states. AUX ON remains protected by the anti-chatter interval.
- Transactional NVS slot verification now also verifies every per-channel `inv0..inv5` output-inversion key.
- GPIO5 is documented as an ESP32 strapping pin; the concrete relay input circuit must preserve the required boot-time strap level.
- Release documentation and wiring map were refreshed for V0.15.10.


## V0.15.11 — AUX Post-REACH
- Added programmable AUX behavior after a missed COMFORT REACH deadline.
- Policies: OFF, continue for a configured number of minutes, or continue until COMFORT.
- Added an independent Post-REACH maximum runtime safety limit.
- Calendar priority is preserved: the next calendar event stops Post-REACH assistance.
- Settings are stored transactionally with the existing AUX configuration and exposed to Web UI and Android.

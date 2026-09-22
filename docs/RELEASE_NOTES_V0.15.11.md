# HeatControl V0.15.11

## AUX Post-REACH

This release adds programmable behavior for the case where a COMFORT REACH deadline is reached but the target temperature has not yet been achieved.

### Policies
- **OFF** — AUX stops at the REACH deadline (legacy behavior).
- **MINUTES** — AUX continues for the configured number of minutes after the deadline.
- **COMFORT** — AUX continues until the selected channel reaches COMFORT, subject to the Post-REACH safety limit.

### Safety and priority
- The next calendar event always overrides Post-REACH assistance.
- AUX location maximum temperature remains an immediate protective OFF condition.
- Existing total AUX maximum runtime remains an additional continuous-run limit.
- Post-REACH has its own maximum runtime setting.
- Settings are stored transactionally in the existing configuration slots.

### API
`GET/PUT /api/aux` now exposes: `postReachPolicy`, `postReachMinutes`, `postReachMaxSeconds`.

### Compatibility
`CONFIG_VERSION` remains 17. The new fields are optional NVS keys with safe defaults, so existing configurations are not invalidated.

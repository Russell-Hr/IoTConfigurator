# HeatControl V0.15.1 HARDENED

## Fixes
- Fixed Android calendar parser to read the `transition` field.
- Added a 64 KiB custom NVS partition to safely hold the transactional two-slot configuration.
- Reworked REACH scheduling so a future already-achieved comfort target does not activate at midnight; REACH events are cached per day and remain stable for that occurrence.
- REACH + ECONOMY is explicitly treated as START because this heating-only controller has no active cooling guarantee.
- Outdoor DS18B20 must be a dedicated sensor and cannot share a ROM with CH1..CH6.
- Added validation that six channel sensor ROMs are unique.
- Android controller credentials are now encrypted with an Android Keystore AES/GCM key before Room persistence.
- Android backup excludes the credential database.
- PlatformIO Espressif32 dependency is now version-constrained and the custom partition table is included.
- Removed generated Python/pytest cache artifacts from the release package.

## Security note
The ESP32 still exposes HTTP Basic Auth over local HTTP. This release does not pretend that plain HTTP provides transport confidentiality. HTTPS should be treated as a separate deployment-hardening project.

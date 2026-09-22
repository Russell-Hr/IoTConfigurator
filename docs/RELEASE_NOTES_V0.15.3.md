# HeatControl V0.15.3 HARDENED

## Security and reliability hardening

- Android now encrypts both the stored controller username and password with the Android Keystore-backed AES/GCM credential layer. Existing plaintext records remain readable and are transparently re-encrypted on the next repository update.
- Global firmware CORS was removed. The normal Web UI is served by the ESP32 itself; authenticated API calls no longer opt into unrestricted cross-origin requests.
- Manual TIME-mode valve OPEN/CLOSE commands are rejected while valve position is unknown after boot or a reset. The dedicated calibration workflow remains available to establish a known position.
- Existing LIMIT_SWITCH protections, REACH latching, transactional NVS storage, and sensor-assignment diagnostics are retained.
- Python contract tests were expanded for the new credential-storage, CORS, and unknown-position valve invariants.
- Python cache/generated test artifacts are excluded from the release archive.

## Compatibility

- ESP32 persistent configuration format remains `CONFIG_VERSION 17`; no destructive NVS migration is required for this release.
- Android Room schema remains version 1; credential encryption is handled at the repository boundary without a database schema change.
- The V0.15.2 RC2 archive remains the previous immutable reference build.

## Verification

- Host-side contract tests and static source checks are executed before packaging.
- A real ESP32 PlatformIO build and Android Gradle APK build are **not claimed as executed** in this environment because the corresponding toolchains are unavailable here.

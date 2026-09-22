# HeatControl Android app — V0.15.10 HARDENED

Native Android companion for up to 10 HeatControl units.

## V0.15 changes

- IPv4, hostname and IPv6-safe HTTP URL construction.
- mDNS uses the deprecated-compatible `resolveService()` path on all supported Android versions; no API 34 `ServiceInfoCallback` path is used.
- Outdoor sensor assignment can be cleared as well as assigned.
- Older Android versions use the legacy resolver with a separate listener per service.
- Existing ROM-based DS18B20 channel selection, calendar, RTC, system and factory-reset functions retained.

The app communicates with the ESP32 over local HTTP. Credentials are stored in the app's local Room database; this is not a substitute for encrypted transport.

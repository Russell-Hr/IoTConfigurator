# HeatControl V0.15.5 HARDENED — Adaptive REACH

## Adaptive learning

REACH for COMFORT transitions now learns from actual completed heating cycles. The controller records the measured time needed to raise the channel temperature from the value observed at REACH activation to the configured comfort target.

The prediction uses:
- current indoor temperature and comfort target difference;
- per-channel learned minutes per °C;
- outdoor-temperature bins when the dedicated OUTDOOR DS18B20 is available.

A cold-start fallback of 10 min/°C is retained. Learned values are bounded to 3–25 min/°C and final REACH lead remains bounded to 5–120 minutes.

Learning is intentionally not performed when the target is already reached at activation, when the sensor is invalid, or for ECONOMY/REACH. The latter remains START-equivalent for a heating-only controller.

Statistics are stored separately in NVS namespace `reachlearn`, so the existing channel configuration version remains 17. Factory reset clears the learned statistics.

## UI

Web and Android calendar screens now show Adaptive REACH statistics for the selected channel, including sample count, current coefficient and outdoor-temperature availability, with a reset action.

## API

- `GET /api/reach-learning`
- `POST /api/reach-learning/reset?ch=N`

The existing calendar `transition=start|reach` API remains unchanged.

# monitor
Hybrid RobotiX — San Diego, CA

## Hardware
<!-- List sensors, actuators, and other hardware used by this project -->

## Wiring
<!-- Document I2C addresses, pin assignments, and connections -->

## Calibration
<!-- Record calibration values, tuning constants, and test results -->

## Orientation
<!-- Document physical mounting orientation for any sensors -->

## Notes
<!-- Any other project-specific notes, gotchas, or observations -->

## Confidence Formula

Per-zone confidence (0.00–99.99%) computed from `signal_per_spad` and `range_sigma_mm`:

```python
signal_score = min(signal_per_spad / 8000.0, 1.0)
sigma_score  = max(0, 1 - range_sigma_mm / 30.0)
confidence   = (signal_score * 0.6 + sigma_score * 0.4) * 99.99
```

- `SIGNAL_MAX` = 8000.0 kcps/SPAD (typical max for good returns at 400kHz)
- `SIGMA_MAX` = 30.0 mm (anything above this is poor quality)
- Confidence naturally drops with distance — physically meaningful
- target_status not 5 or 9 → confidence = 0.00%

# UNO-Q — Change Log
## Hybrid RobotiX

> *Hybrid RobotiX designs and creates intelligent technologies that empower people facing physical and accessibility challenges to live more independently and achieve more on their own terms.*

All changes to UNO-Q apps, sketches, Python code, and documentation are recorded here.

---

## Unreleased

### Documentation
- Added `CLAUDE.md` — complete coding standards for all sketches and Python app files
  - No single-line if statements
  - Blank lines before/after blocks
  - Positive conditions first
  - `result = None` single return pattern
  - `snake_case` everywhere
  - `***` with units for unavailable sensor readings
  - Do NOT make assumptions about ANYTHING
  - Make all changes to repos first

### Sensor Changes
- **SCD30 removed entirely** — replaced with Sensirion SCD41 (`SensirionI2C SCD4x` library)
  - `matrix-bno055` — SCD41 replaces SCD30, SGP41 retained
  - `matrix-bno055-mux` — SCD41 on mux2 channel 0, `calibrate_scd30()` removed
  - `hub5-bno055` — SCD41 replaces SCD30, calibration removed
  - `matrix-app` — SCD41 replaces SCD30
  - `scd30` app — sketch and Python updated to SCD41
  - `sketch.yaml` — `Adafruit SCD30` replaced with `Sensirion I2C SCD4x (1.1.0)` in all apps

### sketch.yaml Corrections
- Removed duplicate `Adafruit SGP41` entries in `matrix-bno055` and `matrix-bno055-mux`
- Added version numbers to `Sensirion I2C SCD4x` entries
- Removed `Adafruit BusIO` and `Adafruit Unified Sensor` from `scd30/scd41` — not required by Sensirion library

### Code Quality
- All sketches and Python files reformatted to CLAUDE.md standards:
  - No single-line if statements — all expanded to full blocks
  - Blank lines after if/for/while blocks at block boundaries
  - Blank lines before every function definition
  - No blank lines immediately after `:` or `{` block openers
  - No blank lines immediately before `}` or end of block
  - All negative conditions flipped to positive where applicable
  - Guard clauses use negative condition directly (no `if x: pass else:`)
  - `is not None` / `is None` replaced with truthiness checks
  - Column-aligned variable assignments
  - `result = None` single-return pattern in parse functions
- `SCROLLING_ENABLED` guard converted from early return to positive `if` block in all matrix apps
- `!active` continue guard converted to positive `if` body in mux loop in all matrix apps
- `applyDirection()` in robot — inverted logic corrected to positive condition first

### Bug Fixes
- **All matrix apps** — display `***` with correct units for any unavailable sensor reading instead of skipping loop iteration. No such thing as a primary sensor.
- **`matrix-bno055`** — message 2 now always displays even when BNO055 unavailable

---

## 2026-05 (current development)

### New Apps
- **`ei-c`** — Edge Impulse data collector
  - 500-frame collector with interactive labeler/relabeler
  - `visualizer.py` — frame-by-frame labeler with centroid-based direction suggestion
  - `ei-space.py` — disk space calculator for collection planning
  - Frame count configurable via CLI arg

### Changes
- `monitor` — renamed from `monitor-vl53l5cx`
- All project docs updated to `San Diego, CA`
- All project docs (`docs/README.md`) added to all 17 existing projects
- VL53L5CX confidence formula documented in monitor project docs
- `begin_sensor` — `already_started` treated as success
- `visualize.py` — always writes new output file, input never modified

### Robot
- Comprehensive inline comments added to sketch and Python
- All silent error exits eliminated from robot navigation
- Physical layout and power design documented
- Motor 2040 architecture documented
- Confirmed VL53L5CX physical orientation documented

---

## 2026-04 (initial development)

### Apps Added
- `matrix-bno055` — LED matrix display with BNO055 IMU, SCD30, SHT45, AS7343, APDS9999, SGP41
- `matrix-bno055-mux` — Same as matrix-bno055 with dual TCA9548A I2C mux support
- `hub5-bno055` — HUB75 64×32 LED panel with BNO055, SCD30, SHT45, AS7343, APDS9999, SGP41
- `matrix-lis3dh` — LED matrix with LIS3DH accelerometer tap/double-tap/tilt detection
- `matrix-app` — Basic LED matrix scrolling app with SCD30 environmental data
- `robot` — UNO-Q SMARS robot with VL53L5CX obstacle avoidance and BNO055 heading
- `unoq-smars` — UNO-Q SMARS platform with VL53L5CX depth sensing
- `monitor` — VL53L5CX depth map monitoring and data collection
- `validate-vl53l5cx` — VL53L5CX validation and accuracy testing
- `vl53-diag` — VL53L5CX diagnostic tool
- `lis3dh` — LIS3DH accelerometer app
- `lsm6dsox` — LSM6DSOX IMU app
- `scd30` — SCD30 CO2/temperature/humidity sensor app
- `bridge-test` — Arduino Router Bridge connectivity test
- `sparkfun-vl53-test` — SparkFun VL53L5CX library compatibility test
- `valid` — VL53L5CX validation suite
- `ei-c` — Edge Impulse data collector

### Architecture
- All apps follow Bridge architecture: MCU exposes sensor functions via `Bridge.provide()`, Python reads via `Bridge.call()`
- QWIIC/Stemma QT sensors on `Wire1` (I2C bus 1)
- `Bridge.provide()` calls before `Bridge.begin()` in all sketches
- `Bridge.begin()` always last call in `setup()`
- All sensor state variables declared `static` at file scope

---

*Hybrid RobotiX — San Diego, CA*

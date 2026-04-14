# UNO-Q
## Hybrid RobotiX

> *Hybrid RobotiX designs and creates intelligent technologies that empower people facing physical and accessibility challenges to live more independently and achieve more on their own terms.*

Arduino UNO Q application repository for the HybX Development System. Contains all Arduino apps — each with an MCU sketch, Python controller, and app metadata — for the UNO Q board.

---

## Apps

| App | Description |
|-----|-------------|
| `hub5-bno055` | Displays sensor readings on a 64×32 HUB75 RGB LED matrix panel |
| `lis3dh` | LIS3DH accelerometer data via QWIIC |
| `lsm6dsox` | LSM6DSOX IMU data via QWIIC |
| `matrix-app` | Scrolls SCD30 sensor readings on the UNO Q built-in LED matrix |
| `matrix-bno055` | Scrolls SCD30 and BNO055 readings on the built-in LED matrix |
| `matrix-bno055-mux` | Multi-sensor readings via TCA9548A mux on the built-in LED matrix |
| `matrix-lis3dh` | LIS3DH readings on the built-in LED matrix |
| `scd30` | SCD30 CO2, temperature, and humidity via QWIIC |
| `securesmars` | Full SecureSMARS robot — sensors, Mecanum drive, LED matrix display |

---

## App Structure

Every app follows the same structure:

```
app-name/
  app.yaml              — App name, icon, description
  sketch/
    sketch.ino          — MCU code (Arduino RouterBridge)
    sketch.yaml         — Library dependencies
  python/
    main.py             — Python controller
    requirements.txt    — Python dependencies
```

---

## Usage

This repo is managed by the **HybX Development System**. Do not clone or manage it manually — use the HybX commands:

```bash
# Add and configure this board
board add UNO-Q

# Pull repo and sync new apps into ~/Arduino/UNO-Q
board sync

# Preview what board sync would do without making changes
board sync --dry-run

# Start an app
start matrix-bno055

# Build and flash a sketch
build ~/Arduino/UNO-Q/matrix-bno055/sketch/
```

---

## Hardware

**Board:** Arduino UNO Q (4GB) — dual processor
- QRB2210 MPU running Debian Linux
- STM32U585 MCU for real-time control

**Sensors (QWIIC on Wire1):**
- Adafruit SCD30 — CO2, temperature, humidity
- Adafruit SHT45 — high-precision temperature and humidity
- Adafruit BNO055 — 9-DoF absolute orientation IMU
- Adafruit AS7343 — 14-channel spectral/color sensor
- Adafruit APDS9999 — proximity, lux, RGB color
- Adafruit SGP41 — VOC and NOx gas sensor
- TCA9548A — I2C multiplexer for distance sensor array

**Display:**
- Built-in 12×8 Arduino LED Matrix
- 64×32 HUB75 RGB LED panel (hub5-bno055)

---

## Notes

- The QWIIC connector on the UNO Q uses `Wire1`, not `Wire`. Always pass `&Wire1` when initializing QWIIC/I2C sensors.
- Apps run in Docker containers on the Linux side.
- The HybX Development System installer handles all setup — see [HybX-Development-System](https://github.com/hybotix/HybX-Development-System).

---

## License

See LICENSE file.

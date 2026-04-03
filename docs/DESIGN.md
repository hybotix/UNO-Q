# SecureSMARS Design Document
## Hybrid RobotiX

---

## 1. Project Overview

SecureSMARS is a research-grade mobile robot platform built on the SMARS chassis, developed by Hybrid RobotiX. The platform is designed to demonstrate advanced sensor fusion, environmental awareness, and autonomous navigation capabilities from a compact, wheelchair-accessible development environment.

The guiding philosophy is maximum capability through clean architecture: every sensor, every compute node, and every communication channel has a defined role with no overlap and no compromise.

**Personal philosophy:** *"I. WILL. NEVER. GIVE. UP. OR. SURRENDER."*

---

## 2. Hardware Architecture

### 2.1 Primary Compute — Arduino UNO Q (4GB)

The UNO Q is the central brain of the platform. Its dual-architecture design makes it uniquely suited for this role.

**Qualcomm Dragonwing QRB2210 (Linux side):**
- Quad-core ARM Cortex-A55, 2.0 GHz
- 4GB RAM, 16GB eMMC
- Debian Linux OS
- AI acceleration (Adreno GPU, 2× ISP)
- WiFi 5 dual-band, Bluetooth
- USB-C (power delivery, USB host, DisplayPort)
- Runs Python applications via Arduino App Lab (Docker)
- MQTT client — publishes all sensor data to broker

**STMicroelectronics STM32U585 (MCU side):**
- ARM Cortex-M33, 160MHz
- Real-time sensor polling and hardware interface
- Communicates with Linux side via Arduino Bridge (RPC)
- Drives LED matrix display
- Exposes all sensors to Python via Bridge functions

**UNO Q Role:** All sensor data acquisition, data processing, AI inference, MQTT publishing, and display.

---

### 2.2 Locomotion Compute — Arduino Portenta C33 (planned)

The Portenta C33 is dedicated entirely to locomotion — keeping all real-time motor control isolated from the sensor platform.

**Renesas RA6M5:**
- ARM Cortex-M33, 200MHz
- Handles 4-motor drive system
- Handles 4× encoder feedback (8 interrupt pins)
- Runs PID loops for speed and position control
- Controls Yahboom 4-motor + encoder controller via USB
- Manages mux1 distance sensors (5× VL53)
- Communicates with UNO Q via WiFi/MQTT

**Portenta C33 Role:** All locomotion — motors, encoders, PID, distance sensing.

---

### 2.3 MQTT Broker — Raspberry Pi 4B 8GB (pimqtt)

- Runs Mosquitto MQTT broker
- Aggregates all data from UNO Q and Portenta C33
- Hostname: `pimqtt.local`

---

### 2.4 Power

- **Mobile:** 10,000mAh USB-C PD power bank
- **Stationary:** USB-C PD wall adapter (20W+)
- UNO Q requires USB-C Power Delivery to boot — standard USB will not work

---

## 3. Software Architecture

### 3.1 Bridge Pattern

The Arduino Bridge (RPC) connects the UNO Q MCU and Linux sides. The MCU exposes hardware as named functions; Python calls them remotely.

```
Python (Linux)  ──Bridge.call()──▶  MCU (Zephyr/C++)
                ◀──return value──   Bridge.provide()
```

**Key rules:**
- MCU is the hardware interface — it never makes decisions
- Python is the brain — it reads, processes, formats, and acts
- All sensor reads are initiated by Python
- All display updates are sent from Python to MCU

### 3.2 Application Structure

Each app lives in `~/Arduino/<app-name>/` with two components:

```
<app-name>/
  sketch/
    sketch.ino       # MCU code — Bridge functions, sensor drivers
    sketch.yaml      # Library dependencies
  python/
    main.py          # Python controller — reads sensors, formats output
```

### 3.3 Docker Container

Each app runs in a Docker container managed by `arduino-app-cli`. The container:
- Mounts the app directory at `/app`
- Mounts `$HOME` for persistent file storage (e.g. `~/.scd30-calibrated`)
- Is always nuked and rebuilt on `start`/`restart` to prevent stale cache

### 3.4 MQTT Architecture (planned)

```
UNO Q Python  ──MQTT publish──▶  pimqtt (Mosquitto)
Portenta C33  ──MQTT publish──▶  pimqtt (Mosquitto)
              ◀──MQTT subscribe── pimqtt (Mosquitto)
```

Topics (planned):
- `securesmars/environment/#` — all mux2 environmental sensor data
- `securesmars/locomotion/#` — motor speed, encoder position, PID state
- `securesmars/distance/#` — mux1 VL53 distance readings
- `securesmars/status` — system health and mode

---

## 4. Sensor Platform

### 4.1 I2C Architecture

Two SparkFun Qwiic TCA9548A 8-channel I2C multiplexers are used to organize sensors into logical groups. All sensors connect via QWIIC/Stemma QT cables — no soldering required.

```
UNO Q QWIIC (Wire1)
  ├── Mux 1 (0x70) — Distance sensors      [Portenta C33]
  └── Mux 2 (0x71) — Environmental sensors [UNO Q MCU]
```

### 4.2 Mux 1 (0x70) — Distance Sensors

Managed by Portenta C33. Provides full spatial awareness around the robot.

| Ch | Sensor | Library | Measures | Status |
|----|--------|---------|----------|--------|
| 0 | SparkFun VL53L5CX | SparkFun VL53L5CX Arduino Library (1.0.3) | 8×8 zoned ToF depth map | Pending RMA |
| 1 | Adafruit VL53L1X | Adafruit VL53L1X (3.1.2) | Long range distance — front | Ordered |
| 2 | Adafruit VL53L1X | Adafruit VL53L1X (3.1.2) | Long range distance — rear | Ordered |
| 3 | Adafruit VL53L1X | Adafruit VL53L1X (3.1.2) | Long range distance — left | Ordered |
| 4 | Adafruit VL53L1X | Adafruit VL53L1X (3.1.2) | Long range distance — right | Ordered |

**3 channels spare on Mux 1.**

### 4.3 Mux 2 (0x71) — Environmental Sensors

Managed by UNO Q MCU. Organized into logical groups for efficient iteration.

| Ch | Sensor | Library | Measures | Status |
|----|--------|---------|----------|--------|
| 0 | Adafruit SCD30 | Adafruit SCD30 (1.0.11) | CO2 (ppm), temperature (°C), humidity (%) | Connected |
| 1 | Adafruit SHT45 ×2 | Adafruit SHT4x Library (1.0.5) | Temperature (°C), humidity (%) — primary reference | Connected |
| 2 | Adafruit SGP41 | Adafruit SGP41 (1.0.1) | VOC raw, NOx raw | Ordered |
| 3 | Adafruit BME688 | — | Temperature, humidity, pressure, VOC | Ordered |
| 4 | Adafruit ENS161 | — | TVOC index, eCO2, AQI | Ordered |
| 5 | Adafruit AS7343 | Adafruit AS7343 (1.1.0) | 14-channel spectral/color (400–1000nm) | Ordered |
| 6 | Adafruit APDS9999 | Adafruit APDS9999 (1.0.2) | Proximity, lux, RGB+IR color | Ordered |
| 7 | Adafruit BNO055 | Adafruit BNO055 (1.6.3) | 9-DoF orientation — heading, pitch, roll | Connected |

**Mux 2 is fully populated — 0 channels spare.**

#### Mux 2 Logical Groups

```
Ch 0–1  Primary environmental   CO2, temperature, humidity
Ch 2–4  Gas / air quality       VOC, NOx, pressure, TVOC, AQI
Ch 5–6  Light / color           14-channel spectral, RGB+proximity+lux
Ch 7    Orientation             9-DoF IMU
```

### 4.4 SCD30 Calibration

The SCD30 self-heats, causing elevated temperature readings. On first startup, the SHT45 (and eventually the BME688 as a second reference) is used to calculate a temperature offset that is applied to the SCD30 and stored in its non-volatile memory. A flag file (`~/.scd30-calibrated`) prevents recalibration on subsequent starts.

When the BME688 arrives, calibration will be updated to fuse SHT45 + BME688 for a more accurate reference before applying the offset.

---

## 5. Apps

### 5.1 matrix-bno (current development app)

Direct QWIIC — no mux. Used for active development and testing.

**Sensors:** SCD30, SHT45, BNO055, AS7343 (pending), APDS9999 (pending), SGP41 (pending)

**Display:** 3 scrolling LED matrix messages:
1. Temperature (°F/°C), humidity (%), CO2 (ppm)
2. Heading (°), compass point, pitch (°), roll (°)
3. Additional sensor messages as sensors are connected

**Scrolling:** Controlled by `SCROLLING_ENABLED` boolean in both sketch and Python. Set to `False` for production robot operation.

### 5.2 matrix-bno-mux (full platform app)

Full dual-mux architecture. Used when all hardware is in place.

**Sensors:** All mux1 + mux2 sensors as above.

### 5.3 securesmars (planned)

MQTT-connected robot app. Currently blocked by Docker mDNS issue (Arduino issue filed). Will publish all sensor data to `pimqtt` broker.

---

## 6. Development Workflow

### 6.1 Bootstrap

New users copy `scripts/newrepo.bash` to `$HOME/newrepo.bash`, edit the top variables, and run it once:

```bash
cp ~/Repos/GitHub/hybotix/UNO-Q/scripts/newrepo.bash ~/newrepo.bash
# Edit REPO, REPO_DEST, SECRETS_DEST, COMMANDS at the top
bash ~/newrepo.bash
```

After the first `start`, `~/bin/newrepo` is installed automatically.

### 6.2 scripts/newrepo.bash — User-Configurable Variables

Only these variables need editing for a new user:

```bash
REPO_DEST="$HOME/Repos/GitHub/hybotix/UNO-Q"  # Local clone path
REPO="https://github.com/hybotix/UNO-Q.git"    # Repo URL (fork this)
SECRETS_DEST="securesmars"                       # Apps needing secrets.py
COMMANDS="addlib build clean list logs restart start stop"  # Bin commands
```

Everything below the variables is generic infrastructure — no changes needed.

### 6.3 Bin Commands

All commands are versioned Python scripts in `~/bin/`, symlinked to the latest version by `newrepo.bash`.

| Command | Description |
|---------|-------------|
| `start <app>` | Nuke Docker, clear cache, install newrepo, mount $HOME, start app |
| `restart <app>` | Delegates to start |
| `stop` | Stop the running app |
| `logs` | Show live app logs |
| `list` | List available apps |
| `build <app>` | Compile and flash sketch |
| `clean` | Full Docker nuke + cache clear + restart |
| `addlib` | Search, install, list, or upgrade Arduino libraries |

### 6.4 Conventions

- All Python, no bash/shell scripts
- Versioned filenames: `command-vX.Y.Z.py`
- Configuration in variables at top of each script
- Libraries installed from source into `/usr/local`
- All git pushes use PAT embedded in push URL
- Config stored in JSON where applicable

---

## 7. Known Issues

See `docs/KNOWN_ISSUES.md` for current open issues.

---

## 8. Roadmap

### Near Term
- Receive and connect: SGP41, BME688, ENS161, AS7343, APDS9999
- Activate sensors one by one as they arrive (uncomment `begin()` calls)
- Update SCD30 calibration to fuse SHT45 + BME688
- Resolve Docker mDNS issue for securesmars MQTT app
- Complete mux1 distance sensor hardware (VL53L5CX RMA + 4× VL53L1X)

### Medium Term
- Portenta C33 locomotion system
- Yahboom 4-motor + encoder controller integration
- MQTT data pipeline — all sensors publishing to pimqtt
- ROS 2 Jazzy integration on Pi 5 (Basket Pi)

### Long Term
- Full autonomous navigation
- AI-powered environmental hazard detection
- 30-year Hybrid RobotiX vision: for-profit robotics products + non-profit accessibility platform

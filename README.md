# UNO-Q

A collection of scripts and example apps for developing on the Arduino UNO Q without using Arduino App Lab.

## SecureSMARS

SecureSMARS is a fully autonomous, environmentally aware, hardware-encrypted secure robot built on the Arduino UNO Q platform. It is the flagship project of Hybrid RobotiX.

### Vision

SecureSMARS is a SMARS-based tracked robot with hardware-level end-to-end encrypted communications via the Infineon OPTIGA Trust M crypto authentication chip. No other maker robot implements this level of security. All telemetry is encrypted and authenticated at the hardware level — sensor data, motor commands, and navigation data are all secured end to end.

### Architecture

```
SecureSMARS (Arduino UNO Q)          Raspberry Pi 4B 8GB (MQTT Broker)
┌─────────────────────────────┐      ┌──────────────────────────────┐
│ STM32U585 MCU               │      │ Mosquitto MQTT (TLS)         │
│  - Motor control (4x N20)   │      │ Trust M breakout             │
│  - Encoder feedback         │◄────►│  - Verifies incoming data    │
│  - Sensor polling (QWIIC)   │      │  - Decrypts telemetry        │
│                             │      │  - Signs commands            │
│ QRB2210 MPU (Linux)         │      └──────────────────────────────┘
│  - Autonomous navigation    │
│  - MQTT pub/sub (encrypted) │
│  - Trust M breakout         │
│    - Signs telemetry        │
│    - Verifies commands      │
│    - Private key never      │
│      leaves chip            │
└─────────────────────────────┘
```

### Hardware

**Compute:**
- Arduino UNO Q (4GB) — dual processor (QRB2210 MPU + STM32U585 MCU)
- Raspberry Pi 4B 8GB — dedicated MQTT broker

**Motor System:**
- Adafruit Motor Shield V2
- 4x N20 150:1 geared motors with encoders
- 4x N20 compatible wheels

**Security:**
- 2x Adafruit Infineon Trust M (ADA4351) — one on SecureSMARS, one on Pi 4B broker
- ECC NIST P256/P384, SHA-256, TRNG, RSA 1024/2048
- Private keys stored securely in hardware — never exposed

**Sensors (QWIIC chain):**
- Adafruit BNO055 — 9-DoF absolute orientation, onboard sensor fusion
- Adafruit SCD30 — true CO2, temperature, humidity
- Adafruit ENS161 — TVOC, eCO2, AQI (planned)
- Adafruit BME688 — temperature, humidity, pressure, VOC (planned)
- SparkFun VL53L5CX — 8x8 ToF depth map for obstacle avoidance (RMA pending)
- VL53L1X — long range ToF distance sensing (planned)

### Security Model

- **Hardware crypto** — Trust M on both ends, private keys never leave the chip
- **Encrypted MQTT** — TLS on the Mosquitto broker, Trust M handles certificates
- **Authenticated commands** — SecureSMARS only accepts signed commands from verified sources
- **Encrypted telemetry** — all sensor data encrypted end to end
- **Verified identity** — ECC-based mutual authentication between robot and broker

### Status

- [ ] Motor Shield V2 arriving Thursday April 2nd
- [ ] N20 motors and wheels — pending funds (~$70)
- [ ] BNO055 in hand — ready to develop
- [ ] SCD30 working — app example in this repo
- [ ] Trust M integration — planned
- [ ] Mosquitto TLS broker on Pi 4B — planned
- [ ] Encrypted MQTT tunnel — planned
- [ ] Autonomous navigation — planned

## Compatibility

This workflow has been developed and tested on the Arduino UNO Q. It is expected to be largely compatible with the Arduino VENTUNO Q, which shares the same Debian OS, arduino-app-cli, Arduino CLI, and bridge architecture. The FQBN may differ for the VENTUNO Q. Contributions and testing from VENTUNO Q users are welcome.

## Philosophy

The Arduino UNO Q is a powerful dual-processor board combining a Qualcomm QRB2210 MPU running Debian Linux with an STM32U585 MCU for real-time control. However, Arduino's App Lab development environment is immature and lacks basic features like proper library management.

This repo provides a clean, App Lab-free development workflow using:

- **Arduino CLI** — for compiling and flashing MCU sketches
- **arduino-app-cli** — for running Python apps that bridge the MCU and Linux sides
- **SSH** — for all development, directly on the UNO Q over the network
- **Standard Debian tooling** — apt, pip, systemd, everything you already know

## Prerequisites

- Arduino UNO Q with Debian OS
- SSH access to the UNO Q (`ssh arduino@unoq.local`)
- Arduino CLI installed (pre-installed on UNO Q)
- arduino-app-cli installed (pre-installed on UNO Q)
- `~/bin` in your PATH

To add `~/bin` to your PATH if not already there:

```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

## Installing the Scripts

Clone this repo onto the UNO Q:

```bash
mkdir -p ~/Repos/GitHub/hybotix
git clone https://github.com/hybotix/UNO-Q.git ~/Repos/GitHub/hybotix/UNO-Q
```

Create symlinks in `~/bin`:

```bash
mkdir -p ~/bin
ln -s ~/Repos/GitHub/hybotix/UNO-Q/build-v0.0.1.py ~/bin/build
ln -s ~/Repos/GitHub/hybotix/UNO-Q/start-v0.0.1.py ~/bin/start
ln -s ~/Repos/GitHub/hybotix/UNO-Q/restart-v0.0.1.py ~/bin/restart
ln -s ~/Repos/GitHub/hybotix/UNO-Q/stop-v0.0.1.py ~/bin/stop
chmod +x ~/bin/build ~/bin/start ~/bin/restart ~/bin/stop
```

## Scripts

### build

Compiles and flashes a sketch to the STM32U585 MCU using Arduino CLI and OpenOCD via SWD. Automatically generates a `sketch.yaml` profile based on the libraries used during compilation.

```bash
build <sketch_path>
```

Example:

```bash
build ~/Arduino/adafruit_scd30_test/
```

### start

Starts an arduino-app-cli app.

```bash
start <app_path>
```

Example:

```bash
start ~/Arduino/securesmars/
```

### restart

Stops, starts, and shows logs for an arduino-app-cli app in a single command.

```bash
restart <app_path>
```

Example:

```bash
restart ~/Arduino/scd30-app/
```

### stop

Stops a running arduino-app-cli app.

```bash
stop <app_path>
```

Example:

```bash
stop ~/Arduino/scd30-app/
```

## Adding Arduino Libraries

Use Arduino CLI directly — no YAML hand-editing required:

```bash
arduino-cli lib search "Adafruit SCD30"
arduino-cli lib install "Adafruit SCD30"
arduino-cli lib list
arduino-cli lib upgrade
```

## App Structure

Apps that bridge the MCU and Linux/Python sides follow this structure:

```
my-app/
├── app.yaml
├── sketch/
│   ├── sketch.ino
│   └── sketch.yaml
└── python/
    └── main.py
```

### app.yaml

```yaml
name: My App
icon: 🤖
description: A description of what this app does.
```

### sketch.yaml

```yaml
profiles:
  default:
    platforms:
      - platform: arduino:zephyr
    libraries:
      - Arduino_RouterBridge (0.3.0)
      - dependency: Arduino_RPClite (0.2.1)
      - dependency: ArxContainer (0.7.0)
      - dependency: ArxTypeTraits (0.3.2)
      - dependency: DebugLog (0.8.4)
      - dependency: MsgPack (0.4.2)
      - Your_Library (x.y.z)
default_profile: default
```

### sketch.ino

The MCU sketch must include `Arduino_RouterBridge.h` and call `Bridge.begin()` in setup. Functions exposed to Python are registered with `Bridge.provide()`. Functions must be declared before `setup()`.

```cpp
#include <Arduino_RouterBridge.h>

String my_function() {
    return "hello from MCU";
}

void setup() {
    Bridge.begin();
    Bridge.provide("my_function", my_function);
}

void loop() {
}
```

### main.py

The Python side imports `arduino.app_utils` which is injected at runtime by arduino-app-cli. Use `Bridge.call()` to call functions on the MCU side, and `Bridge.provide()` to expose Python functions to the MCU.

```python
from arduino.app_utils import *
import time

def loop():
    result = Bridge.call("my_function")
    print(result)
    time.sleep(2)

App.run(user_loop=loop)
```

### Running an App

```bash
arduino-app-cli app start ~/my-app/
arduino-app-cli app logs ~/my-app/
arduino-app-cli app stop ~/my-app/
```

Or use the `restart` script:

```bash
restart ~/my-app/
```

## Important Notes

- The QWIIC connector on the UNO Q uses `Wire1`, not the default `Wire`. Always pass `&Wire1` when initializing QWIIC/I2C sensors.
- The Arduino BSP serial monitor is currently broken — use `arduino-app-cli monitor` or the Bridge for MCU output instead.
- Apps run in Docker containers on the Linux side — this is how `arduino.app_utils` is made available to Python.
- The `build` script is for standalone sketches. For apps that use the Bridge, use `arduino-app-cli app start` instead.

## Example Apps

### scd30-app

Reads CO2, temperature, and humidity from an Adafruit SCD30 sensor connected via QWIIC and prints the values every 7 seconds.

```
scd30-app/
├── app.yaml
├── sketch/
│   ├── sketch.ino
│   └── sketch.yaml
└── python/
    └── main.py
```

To run:

```bash
restart ~/Arduino/scd30-app/
```

Expected output:

```
CO2: 470.63 ppm
Temp: 26.57 C
Humidity: 48.79 %
```

### imu-app

Reads accelerometer, gyroscope, and temperature data from an Adafruit LSM6DSOX IMU connected via QWIIC. Useful as the foundation for odometry and orientation tracking on a robot like the SMARS.

```
imu-app/
├── app.yaml
├── sketch/
│   ├── sketch.ino
│   └── sketch.yaml
└── python/
    └── main.py
```

To run:

```bash
restart ~/Arduino/imu-app/
```

Expected output:

```
Accel X: -0.18 m/s^2
Accel Y: -8.75 m/s^2
Accel Z: 5.04 m/s^2
Gyro X: 0.01 rad/s
Gyro Y: -0.00 rad/s
Gyro Z: -0.02 rad/s
Temp: 24.77 C
```


### matrix-app

Standalone LED matrix display app — no MQTT, no Python side required. Scrolls SCD30 and BNO055 sensor readings directly on the UNO Q's built-in 12x8 LED matrix. Requires ArduinoGraphics library.

```
matrix-app/
├── app.yaml
└── sketch/
    ├── sketch.ino
    └── sketch.yaml
```

Scrolls: Temperature F → Temperature C → CO2 ppm → Humidity % → Heading

To run:

```bash
restart ~/Arduino/matrix-app/
```

### securesmars

Full SecureSMARS robot app. Publishes SCD30 and BNO055 sensor data to MQTT broker on pimqtt, subscribes to motor commands on `smars/cmd`, and scrolls sensor data on the LED matrix.

```
securesmars/
├── app.yaml
├── sketch/
│   ├── sketch.ino
│   └── sketch.yaml
└── python/
    ├── main.py
    └── secrets.py
```

MQTT topics:
- `smars/scd` — CO2, temperature, humidity
- `smars/bno` — full 9-DoF IMU data
- `smars/cmd` — motor commands (subscribed)

Motor command JSON format:
```json
{"action": "forward", "speed": 128}
{"action": "strafe_left", "speed": 128}
{"action": "rotate_cw", "speed": 128}
{"action": "move", "x": 100, "y": 100, "r": 0}
{"action": "stop"}
```

To run:

```bash
restart ~/Arduino/securesmars/
```

**Note:** Requires Mosquitto MQTT broker running on pimqtt (192.168.1.117). See KNOWN_ISSUES.md for current Docker networking limitation.


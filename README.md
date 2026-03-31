# UNO-Q

A collection of scripts and example apps for developing on the Arduino UNO Q without using Arduino App Lab.

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
ln -s ~/Repos/GitHub/hybotix/UNO-Q/restart-v0.0.1.py ~/bin/restart
ln -s ~/Repos/GitHub/hybotix/UNO-Q/stop-v0.0.1.py ~/bin/stop
chmod +x ~/bin/build ~/bin/restart ~/bin/stop
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


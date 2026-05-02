# robot
Hybrid RobotiX — San Diego, CA

## Hardware

### Sensors
- SparkFun VL53L5CX large breakout (Wire1, I2C) — forward-facing 8x8 ToF depth sensor
- Adafruit BNO055 (Wire1, 0x28) — absolute heading via Euler X

### Motor Controller
- Pimoroni Motor 2040 (RP2040-based)
  - 4x DC motor outputs with encoder inputs
  - Built-in current sensing per motor
  - Runs MicroPython natively
  - Communicates with host over USB serial (line-based command protocol)

### Motors
- 4x Pimoroni N20 motors with encoders (Mecanum wheel configuration)
  - M1 = Front Left
  - M2 = Front Right
  - M3 = Rear Left
  - M4 = Rear Right

### Chassis
- Small tracked/wheeled N20-motor chassis with Mecanum wheels

---

## Architecture

The robot follows the same host/MCU split established by the UNO Q platform:

```
Host Computer (Linux)
    │
    │  USB Serial — line-based command protocol
    ▼
Pimoroni Motor 2040 (MicroPython)
    │
    ├── Motor 1 (FL) ── N20 + encoder
    ├── Motor 2 (FR) ── N20 + encoder
    ├── Motor 3 (RL) ── N20 + encoder
    └── Motor 4 (RR) ── N20 + encoder
```

**Motor 2040 responsibilities:**
- Motor control (speed, direction)
- Encoder reading (odometry data)
- Current sensing (stall detection)
- Exposes a simple line-based serial command interface
- No navigation logic — just hardware abstraction

**Host computer responsibilities:**
- Navigation state machine
- Obstacle avoidance (VL53L5CX)
- Heading control (BNO055)
- AI inferencing (Edge Impulse models)
- All high-level decision making

This mirrors exactly the UNO Q pattern: the MCU owns the hardware,
the Linux side owns the intelligence.

---

## Motor 2040 Command Interface

Line-based serial protocol — one command per line, one response per line.
Commands are case-insensitive. All values are integers.

### Commands

| Command | Description | Response |
|---------|-------------|----------|
| `DRIVE <direction> <speed>` | Drive in direction at speed 0-255 | `ok` |
| `STOP` | Stop all motors immediately | `ok` |
| `SET_SPEED <m1> <m2> <m3> <m4>` | Set individual motor speeds (-255 to 255) | `ok` |
| `GET_ENCODERS` | Read encoder counts for all 4 motors | `<m1>,<m2>,<m3>,<m4>` |
| `RESET_ENCODERS` | Reset all encoder counts to zero | `ok` |
| `GET_CURRENT` | Read motor current (mA) for all 4 motors | `<m1>,<m2>,<m3>,<m4>` |
| `GET_STATUS` | Get motor controller status | `ready` or `error:<msg>` |
| `PING` | Health check | `pong` |

### Direction values (DRIVE command)
| Direction | Description |
|-----------|-------------|
| `forward` | Drive forward |
| `backward` | Drive backward |
| `left` | Strafe left (Mecanum) |
| `right` | Strafe right (Mecanum) |
| `rotate_left` | Rotate left in place |
| `rotate_right` | Rotate right in place |
| `stop` | Stop all motors |

### Error responses
All commands return `error:<message>` on failure. The host must never
silently ignore an error response.

---

## Physical Layout

SMARs modular chassis with layered design:

```
┌─────────────────────────────┐
│  UNO Q                      │  ← Top layer
│  VL53L5CX (front-facing)    │
│  BNO055                     │
└─────────────┬───────────────┘
              │ USB Serial
┌─────────────▼───────────────┐
│  Pimoroni Motor 2040        │  ← Middle layer
└─────────────────────────────┘
┌─────────────────────────────┐
│  4x N20 motors w/ encoders  │  ← Bottom layer
│  LiPo battery pack          │
│  Buck converter (→ 5V)      │
└─────────────────────────────┘
```

## Power Design

Single LiPo battery powers everything:

| Component | Supply |
|-----------|--------|
| Motor 2040 + N20 motors | LiPo direct (2S = 7.4V) |
| UNO Q | Buck converter → 5V from LiPo |

- One battery to charge
- Buck converter steps LiPo down to 5V for UNO Q
- Motor 2040 has built-in regulator powering the RP2040 from motor supply
- Short right-angle USB-C cable between Motor 2040 and UNO Q



### VL53L5CX
| Signal | UNO Q Pin |
|--------|-----------|
| SDA | Wire1 SDA |
| SCL | Wire1 SCL |
| VIN | 3.3V |
| GND | GND |

### BNO055
| Signal | UNO Q Pin |
|--------|-----------|
| SDA | Wire1 SDA |
| SCL | Wire1 SCL |
| VIN | 3.3V |
| GND | GND |
| I2C Address | 0x28 |

### Motor 2040
| Connection | Details |
|------------|---------|
| Host interface | USB Serial |
| Power | External 5V supply |
| Motor outputs | M1=FL, M2=FR, M3=RL, M4=RR |

---

## Orientation

### VL53L5CX (confirmed by physical test)
- Row 0 = top of FOV, Row 7 = bottom of FOV
- Column 0 = robot left, Column 7 = robot right
- Sensor mounted forward-facing on robot front

### BNO055
- Heading from VECTOR_EULER X axis
- 0° = forward (to be calibrated on first run)

---

## Calibration

### BNO055
- Allow 30 seconds of movement for full calibration on first power-up
- Calibration status available via get_imu_status Bridge call

### Motor speeds
- driveSpeed = 128 (default forward/backward speed)
- turnSpeed = 80 (default rotation speed)
- Per-motor INVERT flags may be needed depending on motor wiring

---

## Notes
- Motor 2040 command interface is to be implemented in MicroPython
- The Adafruit Motor Shield V2 used during initial development will be
  replaced by the Motor 2040 + N20 encoder motors from Pimoroni
- Encoder odometry is a planned enhancement for accurate positioning
- Current sensing will be used for stall detection and safety cutoff

---

## Future Enhancements

### Pan/Tilt Platform (VL53L5CX)
The VL53L5CX will eventually mount on a pan/tilt servo platform driven
by an Adafruit PCA9685 PWM Servo Driver (Wire1, 0x40). When added:
- Scanning will pan the sensor instead of rotating the whole robot
- Absolute look direction = pan_angle + BNO055_heading
- Bridge functions to add: set_pan(deg), set_tilt(deg), get_pan(), get_tilt()

### Motor Encoder Odometry
Motors have encoders. When integrated:
- Replace time-based backup (BACKUP_MS) with distance-based odometry
- Replace time-based rotation with encoder-counted turns
- Hookup point in code: handle_obstacle() in robot/python/main.py
- GET_ENCODERS command already defined in Motor 2040 command interface

---

## ML Inferencing

### Edge Impulse (active)
Primary ML platform. Account active. Data collection via ei-c project.
- Input: VL53L5CX 8x8 depth map (64 features per frame)
- Classes: UP, DOWN, LEFT, RIGHT, CENTER (expandable)
- Goal: obstacle classification and spatial awareness for navigation

### Google LiteRT
Google's evolution of TensorFlow Lite. To be evaluated alongside Edge Impulse.
- May be better suited for My Chairiet Basket Pi 5 / Hailo-10H
- Key question: does Hailo-10H support LiteRT models natively?

---

## Navigation State Machine

```
INIT → FORWARD → OBSTACLE → SCANNING → RECOVERING → FULL_BLOCK
                                ↑_____________________________|
```

All navigation logic runs on the Linux side (Python). The Arduino sketch
handles hardware I/O only via Bridge functions. No navigation logic lives
on the MCU — this separation must be preserved.

/*
 * Robot
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Mecanum-wheel robot with VL53L5CX ToF obstacle detection and
 * BNO055 IMU-guided scan-and-recover navigation.
 *
 * Hardware:
 *   - Adafruit Motor Shield V2 (I2C 0x60) — M1=FL, M2=FR, M3=RL, M4=RR
 *   - SparkFun VL53L5CX large breakout (Wire1) — forward-facing, 8x8 mode
 *   - Adafruit BNO055 (Wire1, 0x28) — absolute heading via Euler angles
 *
 * Future hardware (not yet implemented):
 *   - Adafruit PCA9685 PWM Servo Driver (Wire1, 0x40) — pan/tilt platform
 *     for the VL53L5CX. When added, scanning will pan the sensor rather
 *     than rotating the whole robot.
 *
 * CONFIRMED WORKING PATTERN:
 *   1. Wire1.begin() BEFORE Bridge.begin()
 *   2. Bridge.begin() + Bridge.provide() in setup()
 *   3. begin_sensor() triggered from Linux via Bridge call
 *   4. begin_imu() triggered from Linux via Bridge call
 *   5. Python drives all navigation logic via Bridge calls
 *
 * Mecanum motion matrix (X-pattern wheels, viewed from above):
 *   The X pattern means FL/RR rollers are parallel, FR/RL rollers are parallel.
 *   Opposing diagonal pairs always spin in the same direction.
 *
 *   Forward:      FL=FWD  FR=FWD  RL=FWD  RR=FWD
 *   Reverse:      FL=BWD  FR=BWD  RL=BWD  RR=BWD
 *   Strafe Left:  FL=BWD  FR=FWD  RL=FWD  RR=BWD
 *   Strafe Right: FL=FWD  FR=BWD  RL=BWD  RR=FWD
 *   Rotate CW:    FL=FWD  FR=BWD  RL=FWD  RR=BWD
 *   Rotate CCW:   FL=BWD  FR=FWD  RL=BWD  RR=FWD
 *   Stop:         ALL=RELEASE
 *
 * Bridge functions exposed to Python:
 *   begin_sensor()           -- upload VL53L5CX firmware and start ranging
 *   get_sensor_status()      -- "idle"|"uploading"|"ready"|"init_failed:step:code"
 *   set_resolution(String)   -- "4x4" or "8x8". Returns active resolution string.
 *   get_distance_data()      -- NxN CSV distance matrix, rows separated by ";"
 *   get_target_status()      -- NxN T/F validity matrix, rows separated by ";"
 *   get_signal_data()        -- 8x8 CSV signal_per_spad matrix
 *   get_sigma_data()         -- 8x8 CSV range_sigma_mm matrix
 *   begin_imu()              -- initialize BNO055, returns "ready"|"init_failed"
 *   get_imu_status()         -- "idle"|"ready"|"init_failed"
 *   get_heading()            -- Euler X heading 0.0-360.0 as String, or "0" if not ready
 *   drive(String)            -- "forward"|"reverse"|"left"|"right"|
 *                               "rotate_cw"|"rotate_ccw"|"stop"
 *   set_speed(String)        -- set drive/strafe speed 0-255, returns "ok"
 *   set_turn_speed(String)   -- set rotation speed 0-255, returns "ok"
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>
#include <Adafruit_MotorShield.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// ── Motor inversion flags ─────────────────────────────────────────────────────
// Set a flag to true if that motor spins the wrong direction.
// This lets you correct wiring issues in software without rewiring.
static const bool INVERT_M1 = false;  // Front Left
static const bool INVERT_M2 = false;  // Front Right
static const bool INVERT_M3 = false;  // Rear Left
static const bool INVERT_M4 = false;  // Rear Right

// ── Speed constants ───────────────────────────────────────────────────────────
// driveSpeed: PWM value for forward, reverse, and strafe (0-255, 128 = half)
// turnSpeed:  PWM value for CW/CCW rotation — kept slower for control
// Both are runtime-adjustable via set_speed() and set_turn_speed().
static uint8_t driveSpeed = 128;
static uint8_t turnSpeed  = 80;

// ── Motor shield ──────────────────────────────────────────────────────────────
// Adafruit Motor Shield V2 at default I2C address 0x60.
// Pointers are null until setup() calls shield.begin().
static Adafruit_MotorShield shield;
static Adafruit_DCMotor    *motorFL = nullptr;  // M1 — Front Left
static Adafruit_DCMotor    *motorFR = nullptr;  // M2 — Front Right
static Adafruit_DCMotor    *motorRL = nullptr;  // M3 — Rear Left
static Adafruit_DCMotor    *motorRR = nullptr;  // M4 — Rear Right

// ── VL53L5CX state ───────────────────────────────────────────────────────────
// Firmware upload happens once via begin_sensor() from Python.
// currentResolution tracks 16 (4x4) or 64 (8x8) zones.
static hybx_vl53l5cx sensor;
static uint8_t       currentResolution = 64;    // default 8x8
static bool          sensorBeginCalled = false;  // prevent double-init
static bool          sensorInitFailed  = false;  // true if begin() failed
static bool          sensorInitDone    = false;  // true once begin() returned

// ── BNO055 state ─────────────────────────────────────────────────────────────
// BNO055 on Wire1 at address 0x28 (ADR pin low).
// Euler X gives absolute magnetic heading 0-360°.
static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
static bool            imuInitDone   = false;  // true once begin_imu() returned
static bool            imuInitFailed = false;  // true if bno.begin() failed

// ── Motor helpers ─────────────────────────────────────────────────────────────

/*
 * applyDirection() — applies the per-motor inversion flag.
 * FORWARD <-> BACKWARD are swapped when invert is true.
 * RELEASE and BRAKE pass through unchanged.
 */
static uint8_t applyDirection(uint8_t dir, bool invert) {
    if (!invert) return dir;

    if (dir == FORWARD)  return BACKWARD;
    if (dir == BACKWARD) return FORWARD;

    return dir;
}

/*
 * setMotor() — sets speed and direction on one motor, applying inversion.
 * Always set speed before run() per Adafruit library requirements.
 */
static void setMotor(Adafruit_DCMotor *m, uint8_t dir, bool invert, uint8_t speed) {
    m->setSpeed(speed);
    m->run(applyDirection(dir, invert));
}

/*
 * stopAll() — releases all four motors immediately.
 * RELEASE removes power; the robot coasts to a stop.
 * Call this before any state transition that stops movement.
 */
static void stopAll() {
    motorFL->run(RELEASE);
    motorFR->run(RELEASE);
    motorRL->run(RELEASE);
    motorRR->run(RELEASE);
}

// ── Bridge: drive ─────────────────────────────────────────────────────────────

/*
 * drive() — execute a motion command on all four Mecanum wheels.
 *
 * Commands:
 *   "forward"    — all wheels forward
 *   "reverse"    — all wheels backward
 *   "left"       — strafe left (FL/RR backward, FR/RL forward)
 *   "right"      — strafe right (FL/RR forward, FR/RL backward)
 *   "rotate_cw"  — spin clockwise in place at turnSpeed
 *   "rotate_ccw" — spin counter-clockwise in place at turnSpeed
 *   "stop"       — release all motors
 *
 * Returns "ok" on success, "error:..." on failure.
 */
String drive(String command) {
    // Guard — motors must be initialized before any drive command
    if (motorFL == nullptr) return "error:motors_not_ready";

    command.trim();  // strip any whitespace from Bridge transport

    if (command == "forward") {
        // All four wheels forward — straight ahead
        setMotor(motorFL, FORWARD,  INVERT_M1, driveSpeed);
        setMotor(motorFR, FORWARD,  INVERT_M2, driveSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, driveSpeed);
        setMotor(motorRR, FORWARD,  INVERT_M4, driveSpeed);

    } else if (command == "reverse") {
        // All four wheels backward — straight reverse
        setMotor(motorFL, BACKWARD, INVERT_M1, driveSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, driveSpeed);
        setMotor(motorRL, BACKWARD, INVERT_M3, driveSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, driveSpeed);

    } else if (command == "left") {
        // Strafe left: FL/RR spin backward, FR/RL spin forward
        // Roller forces cancel longitudinally, add laterally left
        setMotor(motorFL, BACKWARD, INVERT_M1, driveSpeed);
        setMotor(motorFR, FORWARD,  INVERT_M2, driveSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, driveSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, driveSpeed);

    } else if (command == "right") {
        // Strafe right: FL/RR spin forward, FR/RL spin backward
        setMotor(motorFL, FORWARD,  INVERT_M1, driveSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, driveSpeed);
        setMotor(motorRL, BACKWARD, INVERT_M3, driveSpeed);
        setMotor(motorRR, FORWARD,  INVERT_M4, driveSpeed);

    } else if (command == "rotate_cw") {
        // Rotate clockwise in place: left side forward, right side backward
        setMotor(motorFL, FORWARD,  INVERT_M1, turnSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, turnSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, turnSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, turnSpeed);

    } else if (command == "rotate_ccw") {
        // Rotate counter-clockwise: left side backward, right side forward
        setMotor(motorFL, BACKWARD, INVERT_M1, turnSpeed);
        setMotor(motorFR, FORWARD,  INVERT_M2, turnSpeed);
        setMotor(motorRL, BACKWARD, INVERT_M3, turnSpeed);
        setMotor(motorRR, FORWARD,  INVERT_M4, turnSpeed);

    } else if (command == "stop") {
        stopAll();

    } else {
        return "error:unknown_command:" + command;
    }

    return "ok";
}

/*
 * set_speed() — update the drive/strafe PWM speed at runtime.
 * Valid range: 0-255. Returns "ok" or "error:out_of_range".
 */
String set_speed(String val) {
    int v = val.toInt();

    if (v < 0 || v > 255) return "error:out_of_range";

    driveSpeed = (uint8_t)v;
    return "ok";
}

/*
 * set_turn_speed() — update the rotation PWM speed at runtime.
 * Valid range: 0-255. Returns "ok" or "error:out_of_range".
 */
String set_turn_speed(String val) {
    int v = val.toInt();

    if (v < 0 || v > 255) return "error:out_of_range";

    turnSpeed = (uint8_t)v;
    return "ok";
}

// ── Bridge: VL53L5CX ─────────────────────────────────────────────────────────

/*
 * get_sensor_status() — report current VL53L5CX initialization state.
 * Called by Python before and after begin_sensor() to track progress.
 *
 * Returns:
 *   "idle"                  — begin_sensor() not yet called
 *   "uploading"             — begin_sensor() called, firmware uploading
 *   "ready"                 — sensor initialized and ranging
 *   "init_failed:step:code" — initialization failed (step and ULD code)
 *   "error:step:code"       — runtime ranging error
 */
String get_sensor_status() {
    if (!sensorInitDone) return sensorBeginCalled ? "uploading" : "idle";

    if (sensorInitFailed) {
        return "init_failed:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error);
    }

    if (hybx_last_error_step != 0) {
        // Runtime error after successful init
        return "error:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error);
    }

    return "ready";
}

/*
 * begin_sensor() — trigger VL53L5CX firmware upload and start ranging.
 * This blocks for several seconds during firmware upload — Python calls
 * this with a long timeout (120s). Safe to call only once; subsequent
 * calls return "already_started".
 */
String begin_sensor() {
    if (sensorBeginCalled) return "already_started";

    sensorBeginCalled = true;

    if (!sensor.begin()) {
        sensorInitFailed = true;
    }

    sensorInitDone = true;
    return get_sensor_status();
}

/*
 * set_resolution() — switch between 4x4 (16 zone) and 8x8 (64 zone) mode.
 * Only applies if sensor is initialized and healthy.
 * Returns the active resolution string ("4x4" or "8x8").
 */
String set_resolution(String resolution) {
    if (sensorInitDone && !sensorInitFailed) {
        uint8_t requested = (resolution == "4x4") ? 16 : 64;

        if (requested != currentResolution) {
            if (resolution == "4x4") {
                sensor.setResolution(16);
                currentResolution = 16;

            } else if (resolution == "8x8") {
                sensor.setResolution(64);
                currentResolution = 64;
            }
        }
    }

    return (currentResolution == 16) ? "4x4" : "8x8";
}

/*
 * get_distance_data() — return the current distance frame as a
 * semicolon-separated matrix of comma-separated values (mm).
 * Format: "d00,d01,...,d07;d10,...;...;d70,...,d77"
 * Returns "0" if no frame is ready, "error:step:code" on sensor fault.
 */
String get_distance_data() {
    if (!hybx_sensor_ready) {
        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) +
                   ":" + String(hybx_last_error);
        }
        return "0";  // frame not yet available
    }

    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";

    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            result += String(hybx_distance_mm[row][col]);
            if (col < width - 1) result += ",";
        }
        if (row < width - 1) result += ";";
    }

    return result;
}

/*
 * get_target_status() — return validity flags for each zone.
 * "T" = valid reading (ST status 5 or 9), "F" = invalid.
 * Same matrix format as get_distance_data().
 * Returns "0" if no frame is ready.
 */
String get_target_status() {
    if (!hybx_sensor_ready) return "0";

    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";

    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            // ST status codes 5 (valid) and 9 (wrap-around valid) are good reads
            uint8_t st = hybx_target_status[row][col];
            result += (st == 5 || st == 9) ? "T" : "F";
            if (col < width - 1) result += ",";
        }
        if (row < width - 1) result += ";";
    }

    return result;
}

/*
 * get_signal_data() — return signal_per_spad values for all 8x8 zones.
 * Higher values indicate stronger returns (kcps/SPAD).
 * Always 8x8 regardless of resolution setting.
 * Returns "0" if no frame is ready.
 */
String get_signal_data() {
    if (!hybx_sensor_ready) return "0";

    String result = "";

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_signal_per_spad[row][col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }

    return result;
}

/*
 * get_sigma_data() — return range_sigma_mm values for all 8x8 zones.
 * Lower values indicate more precise readings (mm standard deviation).
 * Always 8x8 regardless of resolution setting.
 * Returns "0" if no frame is ready.
 */
String get_sigma_data() {
    if (!hybx_sensor_ready) return "0";

    String result = "";

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_range_sigma_mm[row][col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }

    return result;
}

// ── Bridge: BNO055 ───────────────────────────────────────────────────────────

/*
 * begin_imu() — initialize the BNO055 on Wire1.
 * Safe to call multiple times — returns current status if already called.
 * Returns "ready" or "init_failed".
 */
String begin_imu() {
    if (imuInitDone) return imuInitFailed ? "init_failed" : "ready";

    if (!bno.begin()) {
        imuInitFailed = true;  // no BNO055 found or I2C fault
    }

    imuInitDone = true;
    return imuInitFailed ? "init_failed" : "ready";
}

/*
 * get_imu_status() — report current BNO055 state.
 * Returns "idle" | "ready" | "init_failed".
 */
String get_imu_status() {
    if (!imuInitDone) return "idle";

    return imuInitFailed ? "init_failed" : "ready";
}

/*
 * get_heading() — return absolute magnetic heading from BNO055 Euler X.
 * Range: 0.0 to 360.0 degrees. 0° = magnetic north.
 * Returns "0" if IMU is not ready.
 */
String get_heading() {
    if (!imuInitDone || imuInitFailed) return "0";

    sensors_event_t event;
    // VECTOR_EULER: X = heading (yaw), Y = roll, Z = pitch
    bno.getEvent(&event, Adafruit_BNO055::VECTOR_EULER);

    return String(event.orientation.x, 2);  // 2 decimal places
}

// ── Setup / Loop ─────────────────────────────────────────────────────────────

void setup() {
    // Wire1 must be started before Bridge.begin() — Bridge uses I2C internally
    Wire1.begin();

    // Initialize motor shield and get motor objects.
    // All motors are stopped immediately after initialization.
    shield.begin();
    motorFL = shield.getMotor(1);  // M1 — Front Left
    motorFR = shield.getMotor(2);  // M2 — Front Right
    motorRL = shield.getMotor(3);  // M3 — Rear Left
    motorRR = shield.getMotor(4);  // M4 — Rear Right
    stopAll();                     // safety — ensure motors off at startup

    // Register all Bridge functions before Bridge.begin()
    Bridge.begin();
    Bridge.provide("begin_sensor",      begin_sensor);
    Bridge.provide("get_sensor_status", get_sensor_status);
    Bridge.provide("set_resolution",    set_resolution);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);
    Bridge.provide("get_signal_data",   get_signal_data);
    Bridge.provide("get_sigma_data",    get_sigma_data);
    Bridge.provide("begin_imu",         begin_imu);
    Bridge.provide("get_imu_status",    get_imu_status);
    Bridge.provide("get_heading",       get_heading);
    Bridge.provide("drive",             drive);
    Bridge.provide("set_speed",         set_speed);
    Bridge.provide("set_turn_speed",    set_turn_speed);
}

void loop() {
    // Poll the VL53L5CX for new frames only after successful initialization.
    // sensor.poll() checks the data-ready pin and updates the hybx_ arrays.
    if (sensorInitDone && !sensorInitFailed) {
        sensor.poll();
    }
    // Bridge event handling is managed internally by Arduino_RouterBridge.
    // All navigation logic runs in Python — this loop stays minimal.
}

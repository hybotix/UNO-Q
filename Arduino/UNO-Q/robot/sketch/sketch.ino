/*
 * HybX UNO Q Robot
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * An autonomous Mecanum-wheel robot built on the Arduino UNO Q and the
 * HybX Development System. Navigation logic runs on the Linux side in
 * Python; the sketch handles all hardware access and exposes it to Python
 * via the Arduino RouterBridge.
 *
 * Hardware:
 *   - Adafruit Motor Shield V2 (I2C 0x60) — M1=FL, M2=FR, M3=RL, M4=RR
 *   - SparkFun VL53L5CX large breakout (Wire1) — forward-facing, 8x8 mode
 *   - Adafruit BNO055 (Wire1, 0x28) — absolute heading via Euler angles
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
 *   get_heading()            -- Euler X heading 0.0-360.0 as String, or "0"
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

static const bool INVERT_M1 = false;
static const bool INVERT_M2 = false;
static const bool INVERT_M3 = false;
static const bool INVERT_M4 = false;

static uint8_t drive_speed = 128;
static uint8_t turn_speed  = 80;

static Adafruit_MotorShield shield;
static Adafruit_DCMotor    *motor_fl = nullptr;
static Adafruit_DCMotor    *motor_fr = nullptr;
static Adafruit_DCMotor    *motor_rl = nullptr;
static Adafruit_DCMotor    *motor_rr = nullptr;

static hybx_vl53l5cx sensor;
static uint8_t       current_resolution = 64;
static bool          sensor_begin_called = false;
static bool          sensor_init_failed  = false;
static bool          sensor_init_done    = false;

static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
static bool            imu_init_done   = false;
static bool            imu_init_failed = false;

static uint8_t applyDirection(uint8_t dir, bool invert) {

    // Apply motor direction inversion
    if (invert) {

        // Invert direction
        if (dir == FORWARD) {
            return BACKWARD;
        }

        // Invert direction
        if (dir == BACKWARD) {
            return FORWARD;
        }
    }

    return dir;
}

static void setMotor(Adafruit_DCMotor *m, uint8_t dir, bool invert, uint8_t speed) {
    m->setSpeed(speed);
    m->run(applyDirection(dir, invert));
}

static void stop_all() {
    motor_fl->run(RELEASE);
    motor_fr->run(RELEASE);
    motor_rl->run(RELEASE);
    motor_rr->run(RELEASE);
}

String drive(String command) {

    // Motors not ready
    if (motor_fl == nullptr) {
        return "error:motors_not_ready";
    }

    command.trim();

    // Check condition
    if (command == "forward") {
        setMotor(motor_fl, FORWARD,  INVERT_M1, drive_speed);
        setMotor(motor_fr, FORWARD,  INVERT_M2, drive_speed);
        setMotor(motor_rl, FORWARD,  INVERT_M3, drive_speed);
        setMotor(motor_rr, FORWARD,  INVERT_M4, drive_speed);
    } else if (command == "reverse") {
        setMotor(motor_fl, BACKWARD, INVERT_M1, drive_speed);
        setMotor(motor_fr, BACKWARD, INVERT_M2, drive_speed);
        setMotor(motor_rl, BACKWARD, INVERT_M3, drive_speed);
        setMotor(motor_rr, BACKWARD, INVERT_M4, drive_speed);
    } else if (command == "left") {
        setMotor(motor_fl, BACKWARD, INVERT_M1, drive_speed);
        setMotor(motor_fr, FORWARD,  INVERT_M2, drive_speed);
        setMotor(motor_rl, FORWARD,  INVERT_M3, drive_speed);
        setMotor(motor_rr, BACKWARD, INVERT_M4, drive_speed);
    } else if (command == "right") {
        setMotor(motor_fl, FORWARD,  INVERT_M1, drive_speed);
        setMotor(motor_fr, BACKWARD, INVERT_M2, drive_speed);
        setMotor(motor_rl, BACKWARD, INVERT_M3, drive_speed);
        setMotor(motor_rr, FORWARD,  INVERT_M4, drive_speed);
    } else if (command == "rotate_cw") {
        setMotor(motor_fl, FORWARD,  INVERT_M1, turn_speed);
        setMotor(motor_fr, BACKWARD, INVERT_M2, turn_speed);
        setMotor(motor_rl, FORWARD,  INVERT_M3, turn_speed);
        setMotor(motor_rr, BACKWARD, INVERT_M4, turn_speed);
    } else if (command == "rotate_ccw") {
        setMotor(motor_fl, BACKWARD, INVERT_M1, turn_speed);
        setMotor(motor_fr, FORWARD,  INVERT_M2, turn_speed);
        setMotor(motor_rl, BACKWARD, INVERT_M3, turn_speed);
        setMotor(motor_rr, FORWARD,  INVERT_M4, turn_speed);
    } else if (command == "stop") {
        stop_all();
    } else {
        return "error:unknown_command:" + command;
    }

    return "ok";
}

String set_speed(String val) {
    int v = val.toInt();

    // Validate speed range
    if (v < 0 || v > 255) {
        return "error:out_of_range";
    }

    drive_speed = (uint8_t)v;
    return "ok";
}

String set_turn_speed(String val) {
    int v = val.toInt();

    // Validate speed range
    if (v < 0 || v > 255) {
        return "error:out_of_range";
    }

    turn_speed = (uint8_t)v;
    return "ok";
}

String get_sensor_status() {

    // Sensor initialization complete
    if (sensor_init_done) {

        // Initialization failed — report error
        if (sensor_init_failed) {
            return "init_failed:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        // Check for sensor error
        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        return "ready";
    }

    return sensor_begin_called ? "uploading" : "idle";
}

String begin_sensor() {

    // Check if sensor begin has been called
    if (sensor_begin_called) {
        return "already_started";
    }

    sensor_begin_called = true;

    // Sensor initialized successfully
    if (sensor.begin()) {
        sensor_init_done = true;
    } else {
        sensor_init_failed = true;
        sensor_init_done   = true;
    }
    return get_sensor_status();
}

String set_resolution(String resolution) {
    uint8_t requested;

    // Sensor initialization complete
    if (sensor_init_done && !sensor_init_failed) {
        requested = (resolution == "4x4") ? 16 : 64;

        // Resolution changed — update sensor
        if (requested != current_resolution) {

            // Set requested resolution
            if (resolution == "4x4") {
                sensor.setResolution(16);
                current_resolution = 16;
            } else if (resolution == "8x8") {
                sensor.setResolution(64);
                current_resolution = 64;
            }
        }
    }

    return (current_resolution == 16) ? "4x4" : "8x8";
}

String get_distance_data() {
    int    width;
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {
        width = (current_resolution == 16) ? 4 : 8;

        // Add separator between values
        for (row = 0; row < width; row++) {

            // Add separator between values
            for (col = 0; col < width; col++) {
                result += String(hybx_distance_mm[row][col]);

                // Add separator between values
                if (col < width - 1) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < width - 1) {
                result += ";";
            }
        }

        return result;
    }

    // Check for sensor error
    if (hybx_last_error_step != 0) {
        return "error:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
    }

    return "0";
}

String get_target_status() {
    int     width;
    int     row;
    int     col;
    uint8_t st;
    String  result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {
        width = (current_resolution == 16) ? 4 : 8;

        // Add separator between values
        for (row = 0; row < width; row++) {

            // Add separator between values
            for (col = 0; col < width; col++) {
                st = hybx_target_status[row][col];
                result += (st == 5 || st == 9) ? "T" : "F";

                // Add separator between values
                if (col < width - 1) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < width - 1) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String get_signal_data() {
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                result += String(hybx_signal_per_spad[row][col]);

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String get_sigma_data() {
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                result += String(hybx_range_sigma_mm[row][col]);

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String begin_imu() {

    // Sensor initialization complete
    if (imu_init_done) {
        return imu_init_failed ? "init_failed" : "ready";
    }

    // BNO055 initialized successfully
    if (bno.begin()) {
        imu_init_done = true;
    } else {
        imu_init_failed = true;
        imu_init_done   = true;
    }
    return imu_init_failed ? "init_failed" : "ready";
}

String get_imu_status() {

    // Sensor initialization complete
    if (imu_init_done) {
        return imu_init_failed ? "init_failed" : "ready";
    }

    return "idle";
}

String get_heading() {
    sensors_event_t event;

    // Sensor initialization complete
    if (imu_init_done && !imu_init_failed) {
        bno.getEvent(&event, Adafruit_BNO055::VECTOR_EULER);
        return String(event.orientation.x, 2);
    }

    return "0";
}

void setup() {
    Wire1.begin();
    shield.begin();
    motor_fl = shield.getMotor(1);
    motor_fr = shield.getMotor(2);
    motor_rl = shield.getMotor(3);
    motor_rr = shield.getMotor(4);
    stop_all();
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

    // Sensor initialization complete
    if (sensor_init_done && !sensor_init_failed) {
        sensor.poll();
    }
}

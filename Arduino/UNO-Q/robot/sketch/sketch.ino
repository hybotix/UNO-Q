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

static uint8_t driveSpeed = 128;
static uint8_t turnSpeed  = 80;

static Adafruit_MotorShield shield;
static Adafruit_DCMotor    *motorFL = nullptr;
static Adafruit_DCMotor    *motorFR = nullptr;
static Adafruit_DCMotor    *motorRL = nullptr;
static Adafruit_DCMotor    *motorRR = nullptr;

static hybx_vl53l5cx sensor;
static uint8_t       currentResolution = 64;
static bool          sensorBeginCalled = false;
static bool          sensorInitFailed  = false;
static bool          sensorInitDone    = false;

static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
static bool            imuInitDone   = false;
static bool            imuInitFailed = false;

static uint8_t applyDirection(uint8_t dir, bool invert) {
    if (invert) {
        if (dir == FORWARD) {
            return BACKWARD;
        }

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

static void stopAll() {
    motorFL->run(RELEASE);
    motorFR->run(RELEASE);
    motorRL->run(RELEASE);
    motorRR->run(RELEASE);
}

String drive(String command) {
    if (motorFL == nullptr) {
        return "error:motors_not_ready";
    }

    command.trim();

    if (command == "forward") {
        setMotor(motorFL, FORWARD,  INVERT_M1, driveSpeed);
        setMotor(motorFR, FORWARD,  INVERT_M2, driveSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, driveSpeed);
        setMotor(motorRR, FORWARD,  INVERT_M4, driveSpeed);
    } else if (command == "reverse") {
        setMotor(motorFL, BACKWARD, INVERT_M1, driveSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, driveSpeed);
        setMotor(motorRL, BACKWARD, INVERT_M3, driveSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, driveSpeed);
    } else if (command == "left") {
        setMotor(motorFL, BACKWARD, INVERT_M1, driveSpeed);
        setMotor(motorFR, FORWARD,  INVERT_M2, driveSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, driveSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, driveSpeed);
    } else if (command == "right") {
        setMotor(motorFL, FORWARD,  INVERT_M1, driveSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, driveSpeed);
        setMotor(motorRL, BACKWARD, INVERT_M3, driveSpeed);
        setMotor(motorRR, FORWARD,  INVERT_M4, driveSpeed);
    } else if (command == "rotate_cw") {
        setMotor(motorFL, FORWARD,  INVERT_M1, turnSpeed);
        setMotor(motorFR, BACKWARD, INVERT_M2, turnSpeed);
        setMotor(motorRL, FORWARD,  INVERT_M3, turnSpeed);
        setMotor(motorRR, BACKWARD, INVERT_M4, turnSpeed);
    } else if (command == "rotate_ccw") {
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

String set_speed(String val) {
    int v = val.toInt();

    if (v < 0 || v > 255) {
        return "error:out_of_range";
    }

    driveSpeed = (uint8_t)v;
    return "ok";
}

String set_turn_speed(String val) {
    int v = val.toInt();

    if (v < 0 || v > 255) {
        return "error:out_of_range";
    }

    turnSpeed = (uint8_t)v;
    return "ok";
}

String get_sensor_status() {
    if (sensorInitDone) {
        if (sensorInitFailed) {
            return "init_failed:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        return "ready";
    }

    return sensorBeginCalled ? "uploading" : "idle";
}

String begin_sensor() {
    if (sensorBeginCalled) {
        return "already_started";
    }

    sensorBeginCalled = true;

    if (sensor.begin()) {
        sensorInitDone = true;
    } else {
        sensorInitFailed = true;
        sensorInitDone   = true;
    }
    return get_sensor_status();
}

String set_resolution(String resolution) {
    uint8_t requested;

    if (sensorInitDone && !sensorInitFailed) {
        requested = (resolution == "4x4") ? 16 : 64;

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

String get_distance_data() {
    int    width;
    int    row;
    int    col;
    String result = "";

    if (hybx_sensor_ready) {
        width = (currentResolution == 16) ? 4 : 8;

        for (row = 0; row < width; row++) {
            for (col = 0; col < width; col++) {
                result += String(hybx_distance_mm[row][col]);

                if (col < width - 1) {
                    result += ",";
                }
            }

            if (row < width - 1) {
                result += ";";
            }
        }

        return result;
    }

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

    if (hybx_sensor_ready) {
        width = (currentResolution == 16) ? 4 : 8;

        for (row = 0; row < width; row++) {
            for (col = 0; col < width; col++) {
                st = hybx_target_status[row][col];
                result += (st == 5 || st == 9) ? "T" : "F";

                if (col < width - 1) {
                    result += ",";
                }
            }

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

    if (hybx_sensor_ready) {
        for (row = 0; row < 8; row++) {
            for (col = 0; col < 8; col++) {
                result += String(hybx_signal_per_spad[row][col]);

                if (col < 7) {
                    result += ",";
                }
            }

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

    if (hybx_sensor_ready) {
        for (row = 0; row < 8; row++) {
            for (col = 0; col < 8; col++) {
                result += String(hybx_range_sigma_mm[row][col]);

                if (col < 7) {
                    result += ",";
                }
            }

            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String begin_imu() {
    if (imuInitDone) {
        return imuInitFailed ? "init_failed" : "ready";
    }

    if (bno.begin()) {
        imuInitDone = true;
    } else {
        imuInitFailed = true;
        imuInitDone   = true;
    }
    return imuInitFailed ? "init_failed" : "ready";
}

String get_imu_status() {
    if (imuInitDone) {
        return imuInitFailed ? "init_failed" : "ready";
    }

    return "idle";
}

String get_heading() {
    sensors_event_t event;

    if (imuInitDone && !imuInitFailed) {
        bno.getEvent(&event, Adafruit_BNO055::VECTOR_EULER);
        return String(event.orientation.x, 2);
    }

    return "0";
}

void setup() {
    Wire1.begin();
    shield.begin();
    motorFL = shield.getMotor(1);
    motorFR = shield.getMotor(2);
    motorRL = shield.getMotor(3);
    motorRR = shield.getMotor(4);
    stopAll();
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
    if (sensorInitDone && !sensorInitFailed) {
        sensor.poll();
    }
}

/*
 * VL53L5CX I2C Diagnostic
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Confirms the VL53L5CX is working correctly with the hybx_vl53l5cx
 * library and RouterBridge on the Arduino UNO Q.
 *
 * CONFIRMED WORKING PATTERN (derived from this diagnostic):
 *   1. Wire1.begin() BEFORE Bridge.begin()
 *   2. Bridge.begin() + Bridge.provide() in setup()
 *   3. sensor.begin() triggered from Linux via Bridge call
 *   4. #include <Wire.h> in sketch (not in library)
 *   5. Zephyr native i2c_write()/i2c_write_read() in platform.cpp
 *
 * Python calls begin_sensor() which blocks during firmware upload
 * (~10-30s), then returns pass/fail. After pass, get_diag() confirms.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static String         diag_result  = "idle";
static bool           begin_called = false;
static hybx_vl53l5cx  sensor;

String get_diag() {
    return diag_result;
}

String begin_sensor() {
    // Check if sensor begin has been called
    if (begin_called) {
        return "already_started";
    }

    begin_called = true;
    diag_result  = "uploading";

    // Sensor initialized successfully
    if (sensor.begin()) {
        diag_result = "pass:firmware_uploaded+ranging_started";
    } else {
        diag_result = "fail:step=" + String(hybx_last_error_step) + ":code=" + String(hybx_last_error);
    }

    return diag_result;
}

void setup() {
    Wire1.begin();
    Bridge.provide("get_diag",     get_diag);
    Bridge.provide("begin_sensor", begin_sensor);
}

void loop() {
    delay(10);
}

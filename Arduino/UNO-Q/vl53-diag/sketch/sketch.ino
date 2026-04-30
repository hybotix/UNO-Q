/*
 * VL53L5CX I2C Diagnostic — step 13 (Linux-triggered firmware upload)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Python triggers the firmware upload via begin_sensor().
 * The Bridge is busy during upload — that's OK.
 * Python polls get_diag() after triggering and waits for result.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static String diagResult  = "idle";
static bool   beginCalled = false;
static hybx_vl53l5cx sensor;

String get_diag() {
    return diagResult;
}

String begin_sensor() {
    if (beginCalled) return "already_started";
    beginCalled = true;
    diagResult  = "uploading";
    if (sensor.begin()) {
        diagResult = "pass:firmware_uploaded+ranging_started";
    } else {
        diagResult = "fail:step=" + String(hybx_last_error_step) +
                     ":code=" + String(hybx_last_error) +
                     ":poll=" + String(hybx_init_step);
    }
    return diagResult;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag",    get_diag);
    Bridge.provide("begin_sensor", begin_sensor);
}

void loop() {}

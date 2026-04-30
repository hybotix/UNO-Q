/*
 * VL53L5CX I2C Diagnostic — step 9 (non-blocking firmware upload)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Moves firmware upload to loop() so Bridge.provide() completes
 * immediately. Python polls get_diag() while upload progresses.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static String diagResult = "uploading";
static bool   beginDone  = false;
static hybx_vl53l5cx sensor;

String get_diag() {
    return diagResult;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);
}

void loop() {
    if (!beginDone) {
        beginDone = true;
        if (sensor.begin()) {
            diagResult = "pass:firmware_uploaded+ranging_started";
        } else {
            diagResult = "fail:begin:step=" + String(hybx_last_error_step) +
                         ":code=" + String(hybx_last_error) +
                         ":poll=" + String(hybx_init_step);
        }
    }
}

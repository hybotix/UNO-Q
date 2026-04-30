/*
 * VL53L5CX I2C Diagnostic — step 8 (full firmware upload test)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests the full VL53L5CX firmware upload sequence via Wire1 with Bridge.
 * Mirrors exactly what vl53l5cx_init() does — page selects, firmware
 * write chunks, and the boot poll after upload.
 *
 * Uses hybx_vl53l5cx library directly to test begin().
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static String diagResult = "not_run";
static hybx_vl53l5cx sensor;

String get_diag() {
    return diagResult;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    if (sensor.begin()) {
        diagResult = "pass:firmware_uploaded+ranging_started";
    } else {
        diagResult = "fail:begin:step=" + String(hybx_last_error_step) +
                     ":code=" + String(hybx_last_error) +
                     ":poll=" + String(hybx_init_step);
    }
}

void loop() {}

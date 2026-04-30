/*
 * VL53L5CX I2C Diagnostic — step 5
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Back to the exact pattern that worked in step 2:
 * - Wire.h in sketch
 * - Bridge.begin() first
 * - NO Wire1.begin()
 * - Wire1 used directly
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

void setup() {
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    Wire1.beginTransmission(0x29);
    uint8_t err = Wire1.endTransmission();
    if (err != 0) {
        diagResult = "fail:probe:err=" + String(err);
        return;
    }
    diagResult = "pass:probe_ok";
}

void loop() {}

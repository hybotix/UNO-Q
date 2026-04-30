/*
 * VL53L5CX I2C Diagnostic — step 4
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Wire.h in sketch (not library). Wire1.begin() called BEFORE Bridge.begin().
 * Testing if this ordering works now that Wire.h is in the sketch.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

void setup() {
    Wire1.begin();
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

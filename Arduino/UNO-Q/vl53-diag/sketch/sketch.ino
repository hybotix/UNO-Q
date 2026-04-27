/*
 * VL53L5CX I2C Diagnostic
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Checks if VL53L5CX is present on Wire1 at 0x29 and reports
 * the result via Bridge. Does NOT attempt firmware upload.
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

    Wire1.begin();
    delay(500);

    /* Try a simple I2C probe — just check if 0x29 ACKs */
    Wire1.beginTransmission(0x29);
    uint8_t error = Wire1.endTransmission();

    if (error == 0) {
        diagResult = "found:0x29 ACK";
    } else {
        diagResult = "not_found:error=" + String(error);
    }
}

void loop() {}

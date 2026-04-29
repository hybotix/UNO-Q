/*
 * VL53L5CX I2C Diagnostic
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests Wire1 + Bridge interaction.
 * Wire1.begin() is called BEFORE Bridge.begin() based on sparkfun-vl53-test
 * pattern which is known to work.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

void setup() {
    /* Wire1 MUST be initialized before Bridge.begin() */
    Wire1.begin();
    delay(500);

    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

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

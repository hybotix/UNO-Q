/*
 * VL53L5CX I2C Diagnostic — step 1
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Minimal test: does #include <Wire.h> break Bridge.begin()?
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "bridge_ok";

String get_diag() {
    return diagResult;
}

void setup() {
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);
}

void loop() {}

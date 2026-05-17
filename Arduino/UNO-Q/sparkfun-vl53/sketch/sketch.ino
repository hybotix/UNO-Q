/*
 * SparkFun VL53L5CX Bridge Test
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests whether SparkFun VL53L5CX library can init on Wire1.
 * Reports result via Bridge.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX imager;

static String init_result = "initializing";

String get_status() {
    return init_result;
}

void setup() {
    Wire1.begin();
    delay(1000);

    // Sensor initialization failed
    if (imager.begin(0x29, Wire1)) {
        init_result = "sparkfun_init_ok";
    } else {
        init_result = "sparkfun_init_failed";
    }
    Bridge.provide("get_status", get_status);
}

void loop() {
    delay(10);
}

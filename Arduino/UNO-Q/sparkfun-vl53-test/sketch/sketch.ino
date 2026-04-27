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

static String initResult = "initializing";

String get_status() {
    return initResult;
}

void setup() {
    Bridge.begin();
    Bridge.provide("get_status", get_status);

    Wire1.begin();
    Wire1.setClock(400000);  /* Fast Mode */
    delay(1000);             /* Allow VL53L5CX internal MCU to fully boot */

    if (imager.begin(0x29, Wire1)) {
        initResult = "sparkfun_init_ok";
    } else {
        initResult = "sparkfun_init_failed";
    }
}

void loop() {}

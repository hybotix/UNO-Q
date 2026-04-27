#include <Arduino_RouterBridge.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;

String echo(String value) {
    return value;
}

void setup() {
    Bridge.begin();
    Bridge.provide("echo", echo);
}

void loop() {
}

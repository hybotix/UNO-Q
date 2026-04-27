#include <Arduino_RouterBridge.h>

// Disable all VL53L5CX result fields except distance_mm and target_status
#define VL53L5CX_DISABLE_AMBIENT_PER_SPAD
#define VL53L5CX_DISABLE_NB_TARGET_DETECTED
#define VL53L5CX_DISABLE_NB_SPADS_ENABLED
#define VL53L5CX_DISABLE_SIGNAL_PER_SPAD
#define VL53L5CX_DISABLE_RANGE_SIGMA_MM
#define VL53L5CX_DISABLE_REFLECTANCE_PERCENT
#define VL53L5CX_DISABLE_MOTION_INDICATOR

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

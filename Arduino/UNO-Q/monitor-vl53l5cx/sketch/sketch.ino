/*
 * VL53L5CX Monitor
 * Hybrid RobotiX - Dale Weber (N7PKT)
 *
 * Provides the 8x8 distance array from the SparkFun Qwiic VL53L5CX ToF
 * sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   get_vl53l5cx_data  — returns 64 zone distances as CSV (mm), or "0" if
 *                        data not ready
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on — please wait.
 *
 * Zone order: row-major, zones 0-63 as returned by the ST library.
 * The ST library returns data transposed from the datasheet zone map —
 * the Python side applies the same x/y correction as the SparkFun example
 * to reflect physical reality.
 */

#include <Arduino_RouterBridge.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;

/**
 * Read the 8x8 depth map and return all 64 zone distances as a
 * comma-separated string (mm). Returns "0" if data is not yet ready.
 */
String get_vl53l5cx_data() {
    if (!myImager.isDataReady()) {
        return "0";
    }
    if (!myImager.getRangingData(&measurementData)) {
        return "0";
    }

    String result = "";
    for (int i = 0; i < 64; i++) {
        result += String(measurementData.distance_mm[i]);
        if (i < 63) {
            result += ",";
        }
    }
    return result;
}

void setup() {
    Bridge.begin();
    Wire1.begin();

    // VL53L5CX firmware upload takes ~10 seconds at power-on
    while (!myImager.begin(0x29, Wire1)) {
        delay(100);
    }

    myImager.setResolution(8 * 8);
    myImager.setRangingFrequency(15);
    myImager.startRanging();

    Bridge.provide("get_vl53l5cx_data", get_vl53l5cx_data);
}

void loop() {
}

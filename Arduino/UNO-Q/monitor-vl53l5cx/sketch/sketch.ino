/*
 * VL53L5CX Monitor
 * Hybrid RobotiX - Dale Weber (N7PKT)
 *
 * Provides distance and target status data from the SparkFun Qwiic
 * VL53L5CX ToF sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   set_resolution(int)   — 16 = 4x4, 64 = 8x8
 *   get_distance_data()   — returns NxN matrix of distances in mm
 *   get_target_status()   — returns NxN matrix of True/False validity
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on — please wait.
 *
 * The ST library returns data transposed from the datasheet zone map.
 * The Python side applies the x/y correction to reflect physical reality.
 */

#include <Arduino_RouterBridge.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;
int currentResolution = 64;  // default 8x8

/**
 * Set sensor resolution.
 * resolution: 16 = 4x4, 64 = 8x8
 */
void set_resolution(int resolution) {
    if (resolution != 16 && resolution != 64) {
        return;
    }
    myImager.stopRanging();
    myImager.setResolution(resolution);
    currentResolution = resolution;
    myImager.startRanging();
}

/**
 * Read distance data and return as a row-major matrix string.
 * Rows are separated by ";" and values within each row by ",".
 * Returns "0" if data is not ready.
 */
String get_distance_data() {
    if (!myImager.isDataReady()) {
        return "0";
    }
    if (!myImager.getRangingData(&measurementData)) {
        return "0";
    }

    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";
    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            result += String(measurementData.distance_mm[row * width + col]);
            if (col < width - 1) {
                result += ",";
            }
        }
        if (row < width - 1) {
            result += ";";
        }
    }
    return result;
}

/**
 * Read target status and return as a row-major matrix string.
 * True = valid reading (status 5 or 9), False = invalid.
 * Rows separated by ";", values within row by ",".
 * Returns "0" if data is not ready.
 */
String get_target_status() {
    if (!myImager.isDataReady()) {
        return "0";
    }
    if (!myImager.getRangingData(&measurementData)) {
        return "0";
    }

    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";
    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            uint8_t status = measurementData.target_status[row * width + col];
            result += (status == 5 || status == 9) ? "T" : "F";
            if (col < width - 1) {
                result += ",";
            }
        }
        if (row < width - 1) {
            result += ";";
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

    myImager.setResolution(currentResolution);
    myImager.setRangingFrequency(15);
    myImager.startRanging();

    Bridge.provide("set_resolution",    set_resolution);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);
}

void loop() {
}

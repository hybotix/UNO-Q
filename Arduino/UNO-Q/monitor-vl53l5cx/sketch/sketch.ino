/*
 * VL53L5CX Monitor
 * Hybrid RobotiX — Dale Weber (N7PKT)
 *
 * Provides distance and target status data from the SparkFun Qwiic
 * VL53L5CX ToF sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   set_resolution(String)  — "4x4" or "8x8". Returns "4x4" or "8x8".
 *   get_distance_data()     — returns NxN matrix of distances in mm
 *   get_target_status()     — returns NxN matrix of T/F validity flags
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on — please wait.
 *
 * Bridge functions are registered immediately after Bridge.begin() so the
 * Python side can connect without waiting for sensor initialization.
 * All functions return "0" until the sensor is ready.
 *
 * The ST library returns data transposed from the datasheet zone map.
 * The Python side applies the x/y correction to reflect physical reality.
 *
 * MEMORY ARCHITECTURE
 * -------------------
 * Uses HybX_VL53L5CX (local library) instead of the SparkFun C++ wrapper.
 * All buffers — including the ST ULD's 1452-byte temp_buffer — are static
 * globals (BSS).  operator new is never called, so the RouterBridge String
 * parameter table is never corrupted on Zephyr RTOS.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

#include "HybX_VL53L5CX/HybX_VL53L5CX.h"

/* Sensor driver — all internals in BSS. */
HybX_VL53L5CX vl53;

/* Current resolution (16 = 4x4, 64 = 8x8).  Used by Bridge functions. */
static uint8_t currentResolution = 64;

/**
 * Bridge function: set_resolution
 * Accepts "4x4" or "8x8".
 * Returns the active resolution string.
 */
String vl53_set_resolution(String resolution) {
    if (hybx_sensor_ready) {
        if (resolution == "4x4") {
            vl53.setResolution(16);
            currentResolution = 16;
        } else if (resolution == "8x8") {
            vl53.setResolution(64);
            currentResolution = 64;
        }
    }
    return (currentResolution == 16) ? "4x4" : "8x8";
}

/**
 * Bridge function: get_distance_data
 * Returns row-major matrix string: rows separated by ";", values by ",".
 * Returns "0" if sensor not ready or no data yet.
 */
String get_distance_data() {
    if (!hybx_sensor_ready) {
        return "0";
    }

    int width  = (currentResolution == 16) ? 4 : 8;
    String result = "";

    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            result += String(hybx_distance_mm[row * width + col]);
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
 * Bridge function: get_target_status
 * Returns row-major matrix of T/F: T = valid (status 5 or 9).
 * Returns "0" if sensor not ready or no data yet.
 */
String get_target_status() {
    if (!hybx_sensor_ready) {
        return "0";
    }

    int width  = (currentResolution == 16) ? 4 : 8;
    String result = "";

    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            uint8_t st = hybx_target_status[row * width + col];
            result += (st == 5 || st == 9) ? "T" : "F";
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
    /* Bridge MUST come first. */
    Bridge.begin();

    /* Register all Bridge functions before sensor init so Python can connect
     * immediately.  Functions return "0" until hybx_sensor_ready == true. */
    Bridge.provide("set_resolution",    vl53_set_resolution);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);

    /* Now it is safe to start the sensor (no new/delete from here on). */
    Wire1.begin();
    vl53.begin();   /* uploads firmware; blocks up to 10 s */
}

void loop() {
    vl53.poll();
}

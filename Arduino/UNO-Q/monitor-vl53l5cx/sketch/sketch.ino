/*
 * VL53L5CX Monitor
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Provides distance and target status data from the VL53L5CX ToF
 * sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   get_sensor_status()     -- "ready", "init_failed:<step>:<code>",
 *                              or "initializing"
 *   set_resolution(String)  -- "4x4" or "8x8". Returns active resolution.
 *   get_distance_data()     -- NxN matrix of distances in mm, or "0"
 *   get_target_status()     -- NxN matrix of T/F validity flags, or "0"
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 *
 * INIT SEQUENCE
 * -------------
 * 1. Bridge.begin()
 * 2. Bridge.provide() for ALL functions including get_sensor_status
 *    get_sensor_status returns "initializing" until begin() completes
 * 3. Wire1.begin() + sensor.begin()  (~10s firmware upload)
 * 4. Bridge.provide() is already done — Python gets "initializing"
 *    responses during the upload, then "ready" or "init_failed"
 *
 * This follows the lsm6dsox pattern but registers functions before
 * sensor init so Python gets "initializing" instead of "method not
 * available" during the firmware upload.
 */

#include <Arduino_RouterBridge.h>
#include <hybx_vl53l5cx.h>

hybx_vl53l5cx sensor;

static uint8_t currentResolution = 64;
static bool    initFailed        = false;
static bool    initDone          = false;

String get_sensor_status() {
    if (!initDone)    return "initializing";
    if (initFailed) {
        return "init_failed:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error) +
               ":poll" + String(hybx_init_step);
    }
    if (hybx_sensor_ready) {
        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) +
                   ":" + String(hybx_last_error);
        }
        return "ready";
    }
    return "initializing";
}

String set_resolution(String resolution) {
    if (hybx_sensor_ready) {
        uint8_t requested = (resolution == "4x4") ? 16 : 64;
        if (requested != currentResolution) {
            if (resolution == "4x4") {
                sensor.setResolution(16);
                currentResolution = 16;
            } else if (resolution == "8x8") {
                sensor.setResolution(64);
                currentResolution = 64;
            }
        }
    }
    return (currentResolution == 16) ? "4x4" : "8x8";
}

String get_distance_data() {
    if (!hybx_sensor_ready) return "0";
    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";
    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            result += String(hybx_distance_mm[row * width + col]);
            if (col < width - 1) result += ",";
        }
        if (row < width - 1) result += ";";
    }
    return result;
}

String get_target_status() {
    if (!hybx_sensor_ready) return "0";
    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";
    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            uint8_t st = hybx_target_status[row * width + col];
            result += (st == 5 || st == 9) ? "T" : "F";
            if (col < width - 1) result += ",";
        }
        if (row < width - 1) result += ";";
    }
    return result;
}

void setup() {
    /* Step 1: Start Bridge */
    Bridge.begin();

    /* Step 2: Register ALL functions immediately — get_sensor_status
     * returns "initializing" until initDone is set below */
    Bridge.provide("get_sensor_status",  get_sensor_status);
    Bridge.provide("set_resolution",     set_resolution);
    Bridge.provide("get_distance_data",  get_distance_data);
    Bridge.provide("get_target_status",  get_target_status);

    /* Step 3: Sensor init — Bridge is running and responding with
     * "initializing" during the ~10s firmware upload */
    /* hybx_vl53l5cx uses Zephyr native i2c_transfer() directly.
     * No Wire1.begin() needed — i2c4 is enabled in the DTS overlay. */
    if (!sensor.begin()) {
        initFailed = true;
    }
    initDone = true;
}

void loop() {
    if (initDone && !initFailed) {
        sensor.poll();
    }
}

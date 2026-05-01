/*
 * VL53L5CX Monitor
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Reads 8x8 (or 4x4) depth map from SparkFun Qwiic VL53L5CX ToF sensor
 * and exposes it via the Arduino RouterBridge.
 *
 * CONFIRMED WORKING PATTERN:
 *   1. Wire1.begin() BEFORE Bridge.begin()
 *   2. Bridge.begin() + Bridge.provide() in setup()
 *   3. begin_sensor() triggered from Linux via Bridge call
 *   4. Python polls get_sensor_status() until "ready"
 *   5. Python polls get_distance_data() / get_target_status()
 *
 * Bridge functions:
 *   begin_sensor()          -- trigger firmware upload + start ranging
 *   get_sensor_status()     -- "idle", "uploading", "ready", "init_failed:step:code"
 *   set_resolution(String)  -- "4x4" or "8x8". Returns active resolution.
 *   get_distance_data()     -- NxN CSV matrix of distances in mm, or "0"
 *   get_target_status()     -- NxN T/F validity matrix, or "0"
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static hybx_vl53l5cx sensor;
static uint8_t       currentResolution = 64;
static bool          beginCalled       = false;
static bool          initFailed        = false;
static bool          initDone          = false;

String get_sensor_status() {
    if (!initDone)  return beginCalled ? "uploading" : "idle";
    if (initFailed) {
        return "init_failed:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error);
    }
    if (hybx_last_error_step != 0) {
        return "error:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error);
    }
    return "ready";
}

String begin_sensor() {
    if (beginCalled) return "already_started";
    beginCalled = true;
    if (!sensor.begin()) {
        initFailed = true;
    }
    initDone = true;
    return get_sensor_status();
}

String set_resolution(String resolution) {
    if (initDone && !initFailed) {
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
    if (!hybx_sensor_ready) {
        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) +
                   ":" + String(hybx_last_error);
        }
        return "0";
    }
    int width = (currentResolution == 16) ? 4 : 8;
    String result = "";
    for (int row = 0; row < width; row++) {
        for (int col = 0; col < width; col++) {
            result += String(hybx_distance_mm[row][col]);
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
            uint8_t st = hybx_target_status[row][col];
            result += (st == 5 || st == 9) ? "T" : "F";
            if (col < width - 1) result += ",";
        }
        if (row < width - 1) result += ";";
    }
    return result;
}

String get_signal_data() {
    if (!hybx_sensor_ready) return "0";
    String result = "";
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_signal_per_spad[row][col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }
    return result;
}

String get_sigma_data() {
    if (!hybx_sensor_ready) return "0";
    String result = "";
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_range_sigma_mm[row][col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }
    return result;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("begin_sensor",       begin_sensor);
    Bridge.provide("get_sensor_status",  get_sensor_status);
    Bridge.provide("set_resolution",     set_resolution);
    Bridge.provide("get_distance_data",  get_distance_data);
    Bridge.provide("get_target_status",  get_target_status);
    Bridge.provide("get_signal_data",    get_signal_data);
    Bridge.provide("get_sigma_data",     get_sigma_data);
}

void loop() {
    if (initDone && !initFailed) {
        sensor.poll();
    }
}

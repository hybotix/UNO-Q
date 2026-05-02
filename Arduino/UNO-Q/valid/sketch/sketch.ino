/*
 * VL53L5CX Validation
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Validates VL53L5CX ranging accuracy at user-specified distances.
 * Python drives the validation — sketch just provides sensor data.
 *
 * Bridge functions:
 *   begin_sensor()        -- trigger firmware upload + start ranging
 *   get_sensor_status()   -- "idle", "uploading", "ready", "init_failed:step:code"
 *   get_distance_data()   -- 8x8 CSV distance matrix in mm, or "0"
 *   get_target_status()   -- 8x8 T/F validity matrix, or "0"
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx_unoq.h>

static hybx_vl53l5cx_unoq sensor;
static bool          beginCalled = false;
static bool          initFailed  = false;
static bool          initDone    = false;

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
    if (!sensor.begin()) initFailed = true;
    initDone = true;
    return get_sensor_status();
}

String get_distance_data() {
    if (!hybx_sensor_ready) return "0";
    String result = "";
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_distance_mm[row * 8 + col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }
    return result;
}

String get_target_status() {
    if (!hybx_sensor_ready) return "0";
    String result = "";
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t st = hybx_target_status[row * 8 + col];
            result += (st == 5 || st == 9) ? "T" : "F";
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }
    return result;
}

String get_signal_data() {
    if (!hybx_sensor_ready) return "0";
    String result = "";
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            result += String(hybx_signal_per_spad[row * 8 + col]);
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
            result += String(hybx_range_sigma_mm[row * 8 + col]);
            if (col < 7) result += ",";
        }
        if (row < 7) result += ";";
    }
    return result;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("begin_sensor",      begin_sensor);
    Bridge.provide("get_sensor_status", get_sensor_status);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);
    Bridge.provide("get_signal_data",   get_signal_data);
    Bridge.provide("get_sigma_data",    get_sigma_data);
}

void loop() {
    if (initDone && !initFailed) sensor.poll();
}



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
 *   get_signal_data()     -- 8x8 CSV signal per SPAD matrix, or "0"
 *   get_sigma_data()      -- 8x8 CSV range sigma matrix, or "0"
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

static hybx_vl53l5cx sensor;
static bool          begin_called = false;
static bool          init_failed  = false;
static bool          init_done    = false;

String get_sensor_status() {

    // Sensor initialization complete
    if (init_done) {

        // Initialization failed — report error
        if (init_failed) {
            return "init_failed:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        // Check for sensor error
        if (hybx_last_error_step != 0) {
            return "error:" + String(hybx_last_error_step) + ":" + String(hybx_last_error);
        }

        return "ready";
    }

    return begin_called ? "uploading" : "idle";
}

String begin_sensor() {

    // Check if sensor begin has been called
    if (begin_called) {
        return "already_started";
    }

    begin_called = true;

    // Sensor initialized successfully
    if (sensor.begin()) {
        init_done = true;
    } else {
        init_failed = true;
        init_done   = true;
    }
    return get_sensor_status();
}

String get_distance_data() {
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                result += String(hybx_distance_mm[row * 8 + col]);

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String get_target_status() {
    int     row;
    int     col;
    uint8_t st;
    String  result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                st = hybx_target_status[row * 8 + col];
                result += (st == 5 || st == 9) ? "T" : "F";

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String get_signal_data() {
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                result += String(hybx_signal_per_spad[row * 8 + col]);

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

String get_sigma_data() {
    int    row;
    int    col;
    String result = "";

    // Sensor has valid data — build result string
    if (hybx_sensor_ready) {

        // Add separator between values
        for (row = 0; row < 8; row++) {

            // Add separator between values
            for (col = 0; col < 8; col++) {
                result += String(hybx_range_sigma_mm[row * 8 + col]);

                // Add separator between values
                if (col < 7) {
                    result += ",";
                }
            }

            // Add separator between values
            if (row < 7) {
                result += ";";
            }
        }

        return result;
    }

    return "0";
}

void setup() {
    Wire1.begin();
    Bridge.provide("begin_sensor",      begin_sensor);
    Bridge.provide("get_sensor_status", get_sensor_status);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);
    Bridge.provide("get_signal_data",   get_signal_data);
    Bridge.provide("get_sigma_data",    get_sigma_data);
}

void loop() {

    // Sensor initialization complete
    if (init_done && !init_failed) {
        sensor.poll();
    }
}

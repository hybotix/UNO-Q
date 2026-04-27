/*
 * VL53L5CX Monitor
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Provides distance and target status data from the VL53L5CX ToF
 * sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   get_sensor_status()     -- "ready", "init_failed:<step>:<code>",
 *                              "initializing", or "not_started"
 *   set_resolution(String)  -- "4x4" or "8x8". Returns active resolution.
 *   get_distance_data()     -- NxN matrix of distances in mm, or "0"
 *   get_target_status()     -- NxN matrix of T/F validity flags, or "0"
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on.
 *
 * IMPORTANT: vl53.begin() is called from loop() not setup() so the
 * Bridge remains responsive during the firmware upload. Python polls
 * get_sensor_status() until "ready" before requesting data.
 *
 * hybx_vl53l5cx is installed in ~/Arduino/libraries/hybx_vl53l5cx/
 * and auto-discovered by arduino-cli via dir: entry in sketch.yaml.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

hybx_vl53l5cx sensor;   /* 8x8, address 0x29, Wire1 */

static uint8_t currentResolution = 64;
static bool    initFailed        = false;
static bool    initStarted       = false;

String get_sensor_status() {
    if (!initStarted)      return "not_started";
    if (initFailed) {
        return "init_failed:" + String(hybx_last_error_step) +
               ":" + String(hybx_last_error);
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
        if (resolution == "4x4") {
            sensor.setResolution(16);
            currentResolution = 16;
        } else if (resolution == "8x8") {
            sensor.setResolution(64);
            currentResolution = 64;
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
    /* Register Bridge functions immediately — before sensor init.
     * This keeps the Bridge responsive while the sensor firmware
     * uploads over I2C (up to 10 s). */
    Bridge.begin();
    Bridge.provide("get_sensor_status",  get_sensor_status);
    Bridge.provide("set_resolution",     set_resolution);
    Bridge.provide("get_distance_data",  get_distance_data);
    Bridge.provide("get_target_status",  get_target_status);

    Wire1.begin();
}

void loop() {
    /* Start sensor init on first loop() call so the Bridge is already
     * running and can respond to get_sensor_status() during the upload. */
    if (!initStarted && !initFailed) {
        initStarted = true;
        if (!sensor.begin()) {
            initFailed = true;
        }
    }

    sensor.poll();
}

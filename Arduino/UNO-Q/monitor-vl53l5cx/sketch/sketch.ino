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
 * DESIGN NOTE — init sequence following lsm6dsox pattern
 * -------------------------------------------------------
 * Pattern proven by lsm6dsox sketch:
 *   1. Bridge.begin()          — start Bridge thread
 *   2. sensor init (I2C)       — Bridge running, no functions registered yet
 *   3. Bridge.provide(...)     — register functions after init completes
 *
 * Python connects after Bridge.begin() and polls get_sensor_status()
 * which returns "initializing" until Bridge.provide() registers it.
 * The 60s timeout in main.py covers the ~10s firmware upload.
 *
 * hybx_vl53l5cx is installed in ~/Arduino/libraries/hybx_vl53l5cx/
 * and auto-discovered by arduino-cli via dir: entry in sketch.yaml.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

hybx_vl53l5cx sensor;

static uint8_t currentResolution = 64;
static bool    initFailed        = false;

String get_sensor_status() {
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
    /* Step 1: Start the Bridge thread */
    Bridge.begin();

    /* Step 2: Sensor init while Bridge is running but no functions
     * registered yet. Python will get "method not available" until
     * Bridge.provide() is called — handled by 60s timeout in main.py. */
    Wire1.begin();
    delay(100);
    if (!sensor.begin()) {
        initFailed = true;
    }

    /* Step 3: Register Bridge functions after init completes */
    Bridge.provide("get_sensor_status",  get_sensor_status);
    Bridge.provide("set_resolution",     set_resolution);
    Bridge.provide("get_distance_data",  get_distance_data);
    Bridge.provide("get_target_status",  get_target_status);
}

void loop() {
    if (!initFailed) {
        sensor.poll();
    }
}

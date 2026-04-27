/*
 * VL53L5CX Monitor
 * Hybrid RobotiX — Dale Weber (N7PKT)
 *
 * Provides distance and target status data from the SparkFun Qwiic
 * VL53L5CX ToF sensor via the Arduino RouterBridge.
 *
 * Bridge functions:
 *   set_resolution(String)  -- "4x4" or "8x8". Returns "4x4" or "8x8".
 *   get_distance_data()     -- NxN matrix of distances in mm
 *   get_target_status()     -- NxN matrix of T/F validity flags
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on.
 *
 * Bridge functions are registered before sensor init so the Python side
 * can connect immediately. All functions return "0" until sensor ready.
 *
 * The ST ULD returns data transposed from the datasheet zone map.
 * The Python side applies the x/y correction to reflect physical reality.
 *
 * hybx_vl53l5cx is installed in ~/Arduino/libraries/hybx_vl53l5cx/ and
 * is auto-discovered by arduino-cli. It is not listed in sketch.yaml.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

hybx_vl53l5cx sensor;   /* 8x8, address 0x29, Wire1 */

static uint8_t currentResolution = 64;

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
    if (!hybx_sensor_ready) {
        return "0";
    }
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
    if (!hybx_sensor_ready) {
        return "0";
    }
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
    Bridge.begin();
    Bridge.provide("set_resolution",    set_resolution);
    Bridge.provide("get_distance_data", get_distance_data);
    Bridge.provide("get_target_status", get_target_status);
    Wire1.begin();
    sensor.begin();
}

void loop() {
    sensor.poll();
}

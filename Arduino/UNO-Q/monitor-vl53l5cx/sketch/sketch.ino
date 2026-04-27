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
 * DESIGN NOTE — Bridge responsiveness during sensor init
 * ------------------------------------------------------
 * vl53.begin() uploads sensor firmware over I2C (~10 s) and blocks
 * the calling thread. To keep the Bridge responsive during this time,
 * sensor init runs in a dedicated Zephyr thread. The main thread runs
 * only Bridge.begin() + Bridge.provide() in setup() and sensor.poll()
 * in loop(). The init thread sets initDone when complete.
 *
 * hybx_vl53l5cx is installed in ~/Arduino/libraries/hybx_vl53l5cx/
 * and auto-discovered by arduino-cli via dir: entry in sketch.yaml.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <zephyr/kernel.h>
#include <hybx_vl53l5cx.h>

hybx_vl53l5cx sensor;

static uint8_t currentResolution = 64;
static bool    initFailed        = false;
static bool    initDone          = false;

/* -------------------------------------------------------------------------
 * Zephyr thread for sensor init — runs concurrently with Bridge/loop()
 * -------------------------------------------------------------------------*/
#define INIT_STACK_SIZE 2048
#define INIT_PRIORITY   5

K_THREAD_STACK_DEFINE(init_stack, INIT_STACK_SIZE);
static struct k_thread init_thread;

static void sensor_init_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    if (!sensor.begin()) {
        initFailed = true;
    }
    initDone = true;
}

/* -------------------------------------------------------------------------
 * Bridge functions
 * -------------------------------------------------------------------------*/
String get_sensor_status() {
    if (!initDone) {
        return "initializing";
    }
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

/* -------------------------------------------------------------------------
 * setup / loop
 * -------------------------------------------------------------------------*/
void setup() {
    Bridge.begin();
    Bridge.provide("get_sensor_status",  get_sensor_status);
    Bridge.provide("set_resolution",     set_resolution);
    Bridge.provide("get_distance_data",  get_distance_data);
    Bridge.provide("get_target_status",  get_target_status);

    Wire1.begin();

    /* Launch sensor init in its own thread so Bridge stays responsive */
    k_thread_create(&init_thread, init_stack,
                    K_THREAD_STACK_SIZEOF(init_stack),
                    sensor_init_thread,
                    NULL, NULL, NULL,
                    INIT_PRIORITY, 0, K_NO_WAIT);
}

void loop() {
    if (initDone && !initFailed) {
        sensor.poll();
    }
}

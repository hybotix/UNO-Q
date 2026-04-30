/*
 * VL53L5CX I2C Diagnostic — step 12 (firmware upload with fixed WrMulti)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * WrMulti now correctly sends address+data in single transaction.
 * sensor.begin() runs in Zephyr thread so Bridge stays responsive.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>
#include <zephyr/kernel.h>

static String diagResult = "uploading";
static hybx_vl53l5cx sensor;

String get_diag() {
    return diagResult;
}

#define SENSOR_STACK_SIZE 8192
#define SENSOR_PRIORITY   5
K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread_data;

static void sensor_thread(void *, void *, void *) {
    if (sensor.begin()) {
        diagResult = "pass:firmware_uploaded+ranging_started";
    } else {
        diagResult = "fail:begin:step=" + String(hybx_last_error_step) +
                     ":code=" + String(hybx_last_error) +
                     ":poll=" + String(hybx_init_step);
    }
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    k_thread_create(&sensor_thread_data, sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread, NULL, NULL, NULL,
                    SENSOR_PRIORITY, 0, K_NO_WAIT);
}

void loop() {}

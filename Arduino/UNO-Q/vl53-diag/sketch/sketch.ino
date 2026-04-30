/*
 * VL53L5CX I2C Diagnostic — step 11 (thread isolation test)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Does the Bridge timeout when a Zephyr thread is just sleeping?
 * If yes: the thread itself causes the timeout.
 * If no: it's specifically Wire1 operations in the thread.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <zephyr/kernel.h>

static String diagResult = "thread_running";

String get_diag() {
    return diagResult;
}

#define SENSOR_STACK_SIZE 4096
#define SENSOR_PRIORITY   5
K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread_data;

static void sensor_thread(void *, void *, void *) {
    /* Just sleep — no Wire1 calls */
    for (int i = 0; i < 10; i++) {
        k_msleep(1000);
        diagResult = "sleeping:" + String(i);
    }
    diagResult = "done:no_wire1_calls";
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

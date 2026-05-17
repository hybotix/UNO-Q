/**
 * LIS3DH Sketch
 * Hybrid RobotiX
 *
 * Standalone LIS3DH triple-axis accelerometer app.
 * Python is the controller — reads all sensor data and processes it.
 *
 * Sensors:
 *   - LIS3DH: 3-axis acceleration, tap detection, double-tap detection,
 *             free fall detection
 *
 * Bridge functions exposed to Python:
 *   get_lis3dh_data()     - Read acceleration: returns "x,y,z" in m/s²
 *   get_lis3dh_click()    - Read tap/click status: returns "none", "single", or "double"
 *   get_lis3dh_freefall() - Read free fall status: returns "true" or "false"
 */

#define LIS3DH_ADDR        0x18
#define LIS3DH_RANGE       LIS3DH_RANGE_2_G
#define CLICK_THRESHOLD    80
#define CLICK_TIMELIMIT    10
#define CLICK_TIMELATENCY  20
#define CLICK_TIMEWINDOW   255
#define FREEFALL_THRESHOLD 2.0

#include <Arduino_RouterBridge.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Adafruit_LIS3DH lis3dh = Adafruit_LIS3DH(&Wire1);

String get_lis3dh_data() {
    sensors_event_t event;
    lis3dh.getEvent(&event);
    return String(event.acceleration.x, 4) + "," + String(event.acceleration.y, 4) + "," + String(event.acceleration.z, 4);
}

String get_lis3dh_click() {
    uint8_t click = lis3dh.getClick();

    // No click event
    if (click == 0) {
        return "none";
    }

    // Check click type
    if (click & 0x20) {
        return "double";
    }

    // Check click type
    if (click & 0x10) {
        return "single";
    }

    return "none";
}

String get_lis3dh_freefall() {
    sensors_event_t event;
    float           magnitude;
    lis3dh.getEvent(&event);
    magnitude = sqrt(event.acceleration.x * event.acceleration.x + event.acceleration.y * event.acceleration.y + event.acceleration.z * event.acceleration.z);

    // Magnitude below threshold — free fall
    if (magnitude < FREEFALL_THRESHOLD) {
        return "true";
    }

    return "false";
}

void setup() {

    while (!lis3dh.begin(LIS3DH_ADDR)) {
        delay(100);
    }

    lis3dh.setRange(LIS3DH_RANGE);
    lis3dh.setClick(2, CLICK_THRESHOLD, CLICK_TIMELIMIT, CLICK_TIMELATENCY, CLICK_TIMEWINDOW);

    Bridge.provide("get_lis3dh_data",     get_lis3dh_data);
    Bridge.provide("get_lis3dh_click",    get_lis3dh_click);
    Bridge.provide("get_lis3dh_freefall", get_lis3dh_freefall);
}

void loop() {
    delay(10);
}

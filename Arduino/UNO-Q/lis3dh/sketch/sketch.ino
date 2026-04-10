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
 *   get_lis3dh_data()   - Read acceleration: returns "x,y,z" in m/s²
 *   get_lis3dh_click()  - Read tap/click status: returns "none", "single", or "double"
 *   get_lis3dh_freefall() - Read free fall status: returns "true" or "false"
 */

// ── Configuration ─────────────────────────────────────────────────────────────
#define LIS3DH_ADDR        0x18   // Default I2C address (SA0 low)
#define LIS3DH_RANGE       LIS3DH_RANGE_2_G  // ±2g range
#define CLICK_THRESHOLD    80     // Click sensitivity (for ±2g range, try 40-80)
#define CLICK_TIMELIMIT    10     // Time limit for click in ODR units
#define CLICK_TIMELATENCY  20     // Latency for double click in ODR units
#define CLICK_TIMEWINDOW   255    // Window for double click in ODR units
#define FREEFALL_THRESHOLD 2.0    // Free fall magnitude threshold in m/s²

// ── Includes ──────────────────────────────────────────────────────────────────
#include <Arduino_RouterBridge.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

// ── Sensor instance ───────────────────────────────────────────────────────────
Adafruit_LIS3DH lis3dh = Adafruit_LIS3DH(&Wire1);

// ── Bridge functions ──────────────────────────────────────────────────────────

/**
 * Read LIS3DH 3-axis acceleration data.
 * Returns: "x,y,z" as floats in m/s²
 * Range is configured by LIS3DH_RANGE (default ±2g).
 */
String get_lis3dh_data() {
    sensors_event_t event;
    lis3dh.getEvent(&event);
    return String(event.acceleration.x, 4) + "," +
           String(event.acceleration.y, 4) + "," +
           String(event.acceleration.z, 4);
}

/**
 * Read LIS3DH tap/click detection status.
 * Returns: "none"   — no tap detected
 *          "single" — single tap detected
 *          "double" — double tap detected
 * Click threshold and timing configured by CLICK_* defines above.
 */
String get_lis3dh_click() {
    uint8_t click = lis3dh.getClick();

    if (click == 0) {
        return "none";
    }

    if (click & 0x20) {
        return "double";
    }

    if (click & 0x10) {
        return "single";
    }

    return "none";
}

/**
 * Detect free fall by checking if total acceleration magnitude is near zero.
 * In free fall, gravity disappears and all axes read close to 0 m/s².
 * Returns: "true"  — free fall detected (magnitude < FREEFALL_THRESHOLD)
 *          "false" — no free fall
 */
String get_lis3dh_freefall() {
    sensors_event_t event;
    lis3dh.getEvent(&event);
    float magnitude = sqrt(
        event.acceleration.x * event.acceleration.x +
        event.acceleration.y * event.acceleration.y +
        event.acceleration.z * event.acceleration.z
    );

    if (magnitude < FREEFALL_THRESHOLD) {
        return "true";
    }

    return "false";
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Bridge.begin();

    while (!lis3dh.begin(LIS3DH_ADDR)) {
        delay(100);
    }

    lis3dh.setRange(LIS3DH_RANGE);

    // Configure single and double tap detection
    lis3dh.setClick(2, CLICK_THRESHOLD, CLICK_TIMELIMIT,
                    CLICK_TIMELATENCY, CLICK_TIMEWINDOW);

    // Free fall detected via magnitude check in get_lis3dh_freefall()

    Bridge.provide("get_lis3dh_data",     get_lis3dh_data);
    Bridge.provide("get_lis3dh_click",    get_lis3dh_click);
    Bridge.provide("get_lis3dh_freefall", get_lis3dh_freefall);
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
}

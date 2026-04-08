/**
 * matrix-lis3dh
 * Hybrid RobotiX
 *
 * Scrolls LIS3DH accelerometer data, tap detection, and free fall
 * on the UNO Q LED matrix display.
 * Python is the controller — reads all sensor data, formats messages,
 * and sends them to the MCU for display.
 *
 * Sensors:
 *   - LIS3DH: 3-axis acceleration (m/s²), single tap, double tap, free fall
 *
 * Bridge functions exposed to Python:
 *   get_lis3dh_data()     - Read acceleration: returns "x,y,z" in m/s²
 *   get_lis3dh_click()    - Read tap status: returns "none", "single", "double"
 *   get_lis3dh_freefall() - Read free fall: returns "true" or "false"
 *   set_matrix_msg(msg)   - Set scroll message on LED matrix
 */

// ── Configuration ─────────────────────────────────────────────────────────────
#define LIS3DH_ADDR        0x18   // Default I2C address (SA0 low)
#define LIS3DH_RANGE       LIS3DH_RANGE_2_G  // ±2g range
#define CLICK_THRESHOLD    80     // Click sensitivity (for ±2g, try 40-80)
#define CLICK_TIMELIMIT    10     // Time limit for click in ODR units
#define CLICK_TIMELATENCY  20     // Latency for double click in ODR units
#define CLICK_TIMEWINDOW   255    // Window for double click in ODR units
#define FREEFALL_THRESHOLD 2      // Free fall threshold (~0.35g)
#define FREEFALL_DURATION  5      // Free fall duration in ODR units
#define SCROLL_SPEED_MS    125    // ms per pixel — 125ms is the sweet spot
#define CHAR_WIDTH         6      // Font_5x7 character width including spacing
#define SCROLLING_ENABLED  true   // Set to false to disable scrolling in production

// ── Includes ──────────────────────────────────────────────────────────────────
#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

// ── Sensor and display instances ──────────────────────────────────────────────
Arduino_LED_Matrix matrix;
Adafruit_LIS3DH    lis3dh;

// ── Scroll state machine ──────────────────────────────────────────────────────
static char          matrix_msg[64] = " ... ";
static int           scroll_x       = 12;
static int           msg_pixel_width = 0;
static unsigned long last_scroll_ms  = 0;

/**
 * Recalculate scroll width after message changes.
 * Must be called any time matrix_msg is updated.
 */
void updateScrollMetrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

/**
 * Advance the scroll animation by one pixel if enough time has elapsed.
 * Call from loop() — non-blocking, uses millis() for timing.
 * Has no effect if SCROLLING_ENABLED is false.
 */
void scrollTick() {
    if (!SCROLLING_ENABLED) return;
    if (millis() - last_scroll_ms < SCROLL_SPEED_MS) return;

    last_scroll_ms = millis();

    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    matrix.textFont(Font_5x7);
    matrix.beginText(scroll_x, 1, 0xFFFFFF);
    matrix.print(matrix_msg);
    matrix.endText();
    matrix.endDraw();

    scroll_x--;

    if (scroll_x < -msg_pixel_width) {
        scroll_x = 12;
    }
}

// ── Bridge functions ──────────────────────────────────────────────────────────

/**
 * Read LIS3DH 3-axis acceleration data.
 * Returns: "x,y,z" as floats in m/s²
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
 * Read LIS3DH free fall detection status.
 * Returns: "true"  — free fall detected
 *          "false" — no free fall
 */
String get_lis3dh_freefall() {
    uint8_t int_src = lis3dh.readRegister8(LIS3DH_REG_INT1SRC);

    if (int_src & 0x40) {
        return "true";
    }

    return "false";
}

/**
 * Set the LED matrix scroll message.
 * Parameter: formatted string to display
 * Has no effect if SCROLLING_ENABLED is false.
 */
void set_matrix_msg(String msg) {
    if (!SCROLLING_ENABLED) return;

    matrix.clear();
    msg.toCharArray(matrix_msg, sizeof(matrix_msg));
    updateScrollMetrics();
    scroll_x = 12;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();

    while (!lis3dh.begin(LIS3DH_ADDR, &Wire1)) {
        delay(100);
    }

    lis3dh.setRange(LIS3DH_RANGE);

    // Configure single and double tap detection
    lis3dh.setClick(2, CLICK_THRESHOLD, CLICK_TIMELIMIT,
                    CLICK_TIMELATENCY, CLICK_TIMEWINDOW);

    // Configure free fall detection via INT1
    lis3dh.writeRegister8(LIS3DH_REG_INT1THS, FREEFALL_THRESHOLD);
    lis3dh.writeRegister8(LIS3DH_REG_INT1DUR, FREEFALL_DURATION);
    lis3dh.writeRegister8(LIS3DH_REG_INT1CFG, 0x95);  // Low event on XYZ

    Bridge.provide("get_lis3dh_data",     get_lis3dh_data);
    Bridge.provide("get_lis3dh_click",    get_lis3dh_click);
    Bridge.provide("get_lis3dh_freefall", get_lis3dh_freefall);
    Bridge.provide("set_matrix_msg",      set_matrix_msg);

    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

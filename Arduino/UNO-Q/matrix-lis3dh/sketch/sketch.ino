/**
 * matrix-lis3dh
 * Hybrid RobotiX — v2.1
 *
 * MCU provides LIS3DH sensor data and accepts display content from Python.
 * Python is the controller — it reads sensors, formats messages,
 * and sends them back to the MCU for display on the LED matrix.
 *
 * Bridge functions exposed to Python:
 *   get_lis3dh_data()     - Read acceleration: returns "x,y,z" in m/s²
 *   get_lis3dh_click()    - Read tap status: returns "none", "single", or "double"
 *   get_lis3dh_freefall() - Read free fall: returns "true" or "false"
 *   set_matrix_msg(msg)   - Set scroll message: Python sends formatted string to display
 */

#define LIS3DH_ADDR        0x18
#define LIS3DH_RANGE       LIS3DH_RANGE_2_G
#define CLICK_THRESHOLD    80
#define CLICK_TIMELIMIT    10
#define CLICK_TIMELATENCY  20
#define CLICK_TIMEWINDOW   255
#define SCROLL_SPEED_MS    125
#define CHAR_WIDTH         6
#define SCROLLING_ENABLED  true

#include <Arduino_LED_Matrix.h>
#include <ArduinoGraphics.h>
#include <Arduino_RouterBridge.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

Arduino_LED_Matrix matrix;
Adafruit_LIS3DH    lis3dh = Adafruit_LIS3DH(&Wire1);

static char          matrix_msg[64]  = " ... ";
static int           scroll_x        = 12;
static int           msg_pixel_width = 0;
static unsigned long last_scroll_ms  = 0;

void update_scroll_metrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

void scroll_tick() {

    // Scroll the LED matrix message
    if (SCROLLING_ENABLED) {

        // Throttle scroll rate to SCROLL_SPEED_MS interval
        if (millis() - last_scroll_ms < SCROLL_SPEED_MS) {
            return;
        }

        last_scroll_ms = millis();

        matrix.beginDraw();
        matrix.stroke(0xFFFFFFFF);
        matrix.textFont(Font_5x7);
        matrix.beginText(scroll_x, 1, 0xFFFFFF);
        matrix.print(matrix_msg);
        matrix.endText();
        matrix.endDraw();

        scroll_x--;

        // Message fully scrolled — reset position
        if (scroll_x < -msg_pixel_width) {
            scroll_x = 12;
        }
    }
}

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
    if (magnitude < 2.0) {
        return "true";
    }

    return "false";
}

void set_matrix_msg(String msg) {

    // Scroll the LED matrix message
    if (SCROLLING_ENABLED) {
        matrix.clear();
        msg.toCharArray(matrix_msg, sizeof(matrix_msg));
        update_scroll_metrics();
        scroll_x = 12;
    }
}

void setup() {
    matrix.begin();
    matrix.clear();

    while (!lis3dh.begin(LIS3DH_ADDR)) {
        delay(100);
    }

    lis3dh.setRange(LIS3DH_RANGE);
    lis3dh.setClick(2, CLICK_THRESHOLD, CLICK_TIMELIMIT, CLICK_TIMELATENCY, CLICK_TIMEWINDOW);

    Bridge.provide("get_lis3dh_data",     get_lis3dh_data);
    Bridge.provide("get_lis3dh_click",    get_lis3dh_click);
    Bridge.provide("get_lis3dh_freefall", get_lis3dh_freefall);
    Bridge.provide("set_matrix_msg",      set_matrix_msg);
    Bridge.begin();Bridge.begin();

    update_scroll_metrics();
}

void loop() {
    scroll_tick();
}

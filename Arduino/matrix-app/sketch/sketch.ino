/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * MCU provides sensor data and accepts display content from Python.
 * Python reads sensor data, formats message, sends to matrix.
 *
 * Bridge provides:
 *   get_scd_data()        - SCD30 CSV data (MCU -> Python)
 *   set_matrix_msg(String) - Display string (Python -> MCU)
 */

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
#include <Wire.h>
#include <zephyr/kernel.h>

Arduino_LED_Matrix matrix;
Adafruit_SCD30 scd30;

// ── Scroll state machine ──────────────────────────────────────────────────────
static char matrix_msg[64] = " ... ";
static int scroll_x = 12;
static int msg_pixel_width = 0;
static unsigned long last_scroll_ms = 0;
#define SCROLL_SPEED_MS  50
#define CHAR_WIDTH        6

void updateScrollMetrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

void scrollTick() {
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
String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        return String(scd30.CO2) + "," +
               String(scd30.temperature) + "," +
               String(scd30.relative_humidity);
    }
    return "0,0,0";
}

void set_matrix_msg(String msg) {
    msg.toCharArray(matrix_msg, sizeof(matrix_msg));
    updateScrollMetrics();
    scroll_x = 12;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("set_matrix_msg", set_matrix_msg);
    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

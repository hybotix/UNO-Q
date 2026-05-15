/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * MCU provides sensor data and accepts display content from Python.
 * Python reads sensor data, formats message, sends to matrix.
 *
 * Bridge functions:
 *   get_scd41_data()       - SCD41 CSV data (MCU -> Python)
 *   set_matrix_msg(String) - Display string (Python -> MCU)
 */

#define SCROLL_SPEED_MS  125
#define CHAR_WIDTH       6

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>
#include <zephyr/kernel.h>

Arduino_LED_Matrix matrix;
SensirionI2cScd4x scd41;

static char          matrix_msg[64]  = " ... ";
static int           scroll_x        = 12;
static int           msg_pixel_width = 0;
static unsigned long last_scroll_ms  = 0;

void update_scroll_metrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

void scroll_tick() {
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

    if (scroll_x < -msg_pixel_width) {
        scroll_x = 12;
    }
}

String get_scd41_data() {
    uint16_t co2         = 0;
    float    temperature = 0.0;
    float    humidity    = 0.0;
    bool     data_ready  = false;
    uint16_t error;

    error = scd41.getDataReadyStatus(data_ready);

    if (error || !data_ready) {
        return "0,0,0";
    }

    error = scd41.readMeasurement(co2, temperature, humidity);

    if (error || co2 == 0) {
        return "0,0,0";
    }

    return String(co2) + "," + String(temperature) + "," + String(humidity);
}

void set_matrix_msg(String msg) {
    matrix.clear();
    msg.toCharArray(matrix_msg, sizeof(matrix_msg));
    update_scroll_metrics();
    scroll_x = 12;
}

void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();

    scd41.begin(Wire1, SCD41_I2C_ADDR_62);
    scd41.startPeriodicMeasurement();

    Bridge.provide("get_scd41_data", get_scd41_data);
    Bridge.provide("set_matrix_msg", set_matrix_msg);
    update_scroll_metrics();
}

void loop() {
    scroll_tick();
}

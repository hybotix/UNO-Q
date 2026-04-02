/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * Scrolls SCD30 sensor readings on the UNO Q LED matrix.
 * Bridge provides SCD30 data to Python side.
 *
 * Sensors (QWIIC / Wire1):
 *   - SCD30: CO2, temperature, humidity
 *
 * Bridge provides:
 *   get_scd_data() - SCD30 CSV data
 */

#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoGraphics.h>
#include <Wire.h>
#include <zephyr/kernel.h>

Adafruit_SCD30 scd30;
ArduinoLEDMatrix matrix;

volatile float lastCO2 = 0;
volatile float lastTempC = 0;
volatile float lastHumidity = 0;

#define MATRIX_STACK_SIZE 2048
#define MATRIX_PRIORITY   14

K_THREAD_STACK_DEFINE(matrix_stack, MATRIX_STACK_SIZE);
struct k_thread matrix_thread_data;
struct k_event matrix_event;
static char matrix_msg[64];

void matrixThread(void *p1, void *p2, void *p3) {
    UNUSED(p1); UNUSED(p2); UNUSED(p3);
    while (1) {
        k_event_wait(&matrix_event, 0x001, true, K_FOREVER);
        matrix.beginDraw();
        matrix.stroke(0xFFFFFFFF);
        matrix.textScrollSpeed(50);
        matrix.textFont(Font_5x7);
        matrix.beginText(0, 1, 0xFFFFFF);
        matrix.print(matrix_msg);
        matrix.endText(SCROLL_LEFT);
        matrix.endDraw();
    }
}

String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2      = scd30.CO2;
        lastTempC    = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
        return String(lastCO2) + "," + String(lastTempC) + "," + String(lastHumidity);
    }
    return "0,0,0";
}

void triggerMatrixScroll() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;
    snprintf(matrix_msg, sizeof(matrix_msg),
             " %dF(%dC) %d%% %dppm ",
             (int)round(tempF), (int)round(lastTempC),
             (int)round(lastHumidity), (int)round(lastCO2));
    k_event_set(&matrix_event, 0x001);
}

void setup() {
    // Init Bridge and SCD30 first — proven working pattern
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    Bridge.provide("get_scd_data", get_scd_data);

    // Now add matrix and Zephyr thread
    matrix.begin();
    k_event_init(&matrix_event);
    k_thread_create(&matrix_thread_data, matrix_stack,
                    K_THREAD_STACK_SIZEOF(matrix_stack),
                    matrixThread, NULL, NULL, NULL,
                    MATRIX_PRIORITY, 0, K_NO_WAIT);

    triggerMatrixScroll();
}

void loop() {
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2      = scd30.CO2;
        lastTempC    = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
        triggerMatrixScroll();
    }
}

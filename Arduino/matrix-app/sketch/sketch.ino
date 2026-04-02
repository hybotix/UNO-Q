/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * Scrolls SCD30 sensor readings on the UNO Q LED matrix.
 * BNO055 temporarily disabled.
 *
 * Sensors (QWIIC / Wire1):
 *   - SCD30: CO2, temperature, humidity
 *
 * Bridge provides:
 *   get_scd_data() - SCD30 CSV data
 */

#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
//#include <Adafruit_BNO055.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoGraphics.h>
//#include <utility/imumaths.h>
#include <Wire.h>
#include <zephyr/kernel.h>

Adafruit_SCD30 scd30;
//Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
ArduinoLEDMatrix matrix;

volatile float lastCO2 = 0;
volatile float lastTempC = 0;
volatile float lastHumidity = 0;
//volatile float lastHeading = 0;
//volatile float lastPitch = 0;
//volatile float lastRoll = 0;

#define MATRIX_STACK_SIZE 2048
#define MATRIX_PRIORITY   7

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

//String get_bno_data() { ... }

void triggerMatrixScroll() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;
    snprintf(matrix_msg, sizeof(matrix_msg),
             " %dF %dC %dppm %d%% ",
             (int)round(tempF), (int)round(lastTempC),
             (int)round(lastCO2), (int)round(lastHumidity));
    k_event_set(&matrix_event, 0x001);
}

void setup() {
    Bridge.begin();

    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    //while (!bno.begin()) { delay(100); }
    //bno.setExtCrystalUse(true);

    Bridge.provide("get_scd_data", get_scd_data);
    //Bridge.provide("get_bno_data", get_bno_data);

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

        //sensors_event_t orientationData;
        //bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
        //lastHeading = orientationData.orientation.x;
        //lastPitch   = orientationData.orientation.y;
        //lastRoll    = orientationData.orientation.z;

        triggerMatrixScroll();
    }
}

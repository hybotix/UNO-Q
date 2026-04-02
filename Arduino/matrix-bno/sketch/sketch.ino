/**
 * Matrix BNO Sketch
 * Hybrid RobotiX
 *
 * MCU provides SCD30 and BNO055 data, accepts display content from Python.
 * Python reads sensor data, formats message, sends to matrix.
 *
 * Bridge provides:
 *   get_scd_data()         - SCD30 CSV data
 *   get_bno_data()         - BNO055 CSV data
 *   set_matrix_msg(String) - Display string from Python
 */

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Wire.h>

Arduino_LED_Matrix matrix;
Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);

// ── Scroll state machine ──────────────────────────────────────────────────────
static char matrix_msg[64] = " ... ";
static int scroll_x = 12;
static int msg_pixel_width = 0;
static unsigned long last_scroll_ms = 0;
#define SCROLL_SPEED_MS  125  // Sweet spot — readable without being sluggish
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

String get_bno_data() {
    sensors_event_t orientationData, angVelocityData, linearAccelData, gravityData, magData, accelData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&angVelocityData, Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gravityData, Adafruit_BNO055::VECTOR_GRAVITY);
    bno.getEvent(&magData, Adafruit_BNO055::VECTOR_MAGNETOMETER);
    bno.getEvent(&accelData, Adafruit_BNO055::VECTOR_ACCELEROMETER);
    imu::Quaternion quat = bno.getQuat();
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    int8_t temp = bno.getTemp();

    return String(orientationData.orientation.x) + "," +
           String(orientationData.orientation.y) + "," +
           String(orientationData.orientation.z) + "," +
           String(angVelocityData.gyro.x) + "," +
           String(angVelocityData.gyro.y) + "," +
           String(angVelocityData.gyro.z) + "," +
           String(linearAccelData.acceleration.x) + "," +
           String(linearAccelData.acceleration.y) + "," +
           String(linearAccelData.acceleration.z) + "," +
           String(gravityData.acceleration.x) + "," +
           String(gravityData.acceleration.y) + "," +
           String(gravityData.acceleration.z) + "," +
           String(magData.magnetic.x) + "," +
           String(magData.magnetic.y) + "," +
           String(magData.magnetic.z) + "," +
           String(accelData.acceleration.x) + "," +
           String(accelData.acceleration.y) + "," +
           String(accelData.acceleration.z) + "," +
           String(quat.w(), 4) + "," +
           String(quat.x(), 4) + "," +
           String(quat.y(), 4) + "," +
           String(quat.z(), 4) + "," +
           String(sys) + "," + String(gyro) + "," +
           String(accel) + "," + String(mag) + "," + String(temp);
}

void set_matrix_msg(String msg) {
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
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    while (!bno.begin()) { delay(100); }
    bno.setExtCrystalUse(true);
    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("get_bno_data", get_bno_data);
    Bridge.provide("set_matrix_msg", set_matrix_msg);
    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

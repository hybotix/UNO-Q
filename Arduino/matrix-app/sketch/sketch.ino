/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * MCU reads SCD30, sends data to Python for logging, drives matrix.
 * Correct Bridge pattern — MCU calls Python, not the other way around.
 */

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
#include <Wire.h>

Arduino_LED_Matrix matrix;
Adafruit_SCD30 scd30;

static char matrix_msg[64];

void scrollSensorData(float co2, float tempC, float humidity) {
    float tempF = (tempC * 9.0 / 5.0) + 32.0;
    snprintf(matrix_msg, sizeof(matrix_msg),
             " %dF(%dC) %d%% %dppm ",
             (int)round(tempF), (int)round(tempC),
             (int)round(humidity), (int)round(co2));

    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    matrix.textScrollSpeed(50);
    matrix.textFont(Font_5x7);
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.print(matrix_msg);
    matrix.endText(SCROLL_LEFT);
    matrix.endDraw();
}

void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
}

void loop() {
    if (scd30.dataReady()) {
        scd30.read();
        float co2      = scd30.CO2;
        float tempC    = scd30.temperature;
        float humidity = scd30.relative_humidity;

        // Send data to Python for logging
        String payload = String(co2) + "," + String(tempC) + "," + String(humidity);
        Bridge.call("log_scd_data", payload);

        // Drive the matrix
        scrollSensorData(co2, tempC, humidity);
    }
}

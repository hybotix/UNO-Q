/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * Scrolls SCD30 sensor readings on the UNO Q LED matrix.
 * Non-blocking scroll using millis() state machine — no Zephyr threads.
 * Bridge stays responsive during scrolling.
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

Adafruit_SCD30 scd30;
ArduinoLEDMatrix matrix;

volatile float lastCO2 = 0;
volatile float lastTempC = 0;
volatile float lastHumidity = 0;

// ── Scroll state machine ──────────────────────────────────────────────────────
static char matrix_msg[64];
static int scroll_x = 0;
static int msg_pixel_width = 0;
static unsigned long last_scroll_ms = 0;
#define SCROLL_SPEED_MS  50
#define CHAR_WIDTH        6  // Font_5x7 is 5px wide + 1px spacing

void buildScrollMsg() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;
    snprintf(matrix_msg, sizeof(matrix_msg),
             " %dF(%dC) %d%% %dppm ",
             (int)round(tempF), (int)round(lastTempC),
             (int)round(lastHumidity), (int)round(lastCO2));
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
    scroll_x = 12; // Start just off the right edge of the 12px wide matrix
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
        lastCO2      = scd30.CO2;
        lastTempC    = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
        buildScrollMsg();
        return String(lastCO2) + "," + String(lastTempC) + "," + String(lastHumidity);
    }
    return "0,0,0";
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    Bridge.provide("get_scd_data", get_scd_data);

    matrix.begin();
    buildScrollMsg();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

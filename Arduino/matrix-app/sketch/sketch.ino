/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * Standalone LED matrix display app — no MQTT, no Python side.
 * Scrolls SCD30 and BNO055 sensor readings on the UNO Q LED matrix.
 *
 * Sensors (QWIIC / Wire1):
 *   - SCD30: CO2, temperature, humidity
 *   - BNO055: Heading, pitch, roll
 *
 * LED Matrix scrolls continuously:
 *   Temperature F -> Temperature C -> CO2 ppm -> Humidity % -> Heading
 *
 * Matrix scrolling runs in a dedicated Zephyr thread triggered by k_event,
 * keeping the main loop free for sensor polling and Bridge calls.
 *
 * Bridge provides:
 *   get_scd_data() - SCD30 CSV data
 *   get_bno_data() - BNO055 CSV data
 */

#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoGraphics.h>
#include <utility/imumaths.h>
#include <Wire.h>
#include <zephyr/kernel.h>

// ── Sensor instances ──────────────────────────────────────────────────────────
Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);

// ── LED matrix instance ───────────────────────────────────────────────────────
ArduinoLEDMatrix matrix;

// ── Last known sensor values (shared with matrix thread) ─────────────────────
volatile float lastCO2 = 0;
volatile float lastTempC = 0;
volatile float lastHumidity = 0;
volatile float lastHeading = 0;
volatile float lastPitch = 0;
volatile float lastRoll = 0;

// ── Zephyr thread for non-blocking matrix scrolling ──────────────────────────
#define MATRIX_STACK_SIZE 2048
#define MATRIX_PRIORITY   7

K_THREAD_STACK_DEFINE(matrix_stack, MATRIX_STACK_SIZE);
struct k_thread matrix_thread_data;
struct k_event matrix_event;

// Shared message buffer written by main loop, read by matrix thread
static char matrix_msg[64];

/**
 * Matrix scroll thread — waits for k_event signal, then scrolls
 * the current sensor message. Blocking endText(SCROLL_LEFT) is safe
 * here because it runs in its own Zephyr thread.
 */
void matrixThread(void *p1, void *p2, void *p3) {
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(p3);

    while (1) {
        // Wait indefinitely for a scroll trigger event
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

// ── Sensor data functions ─────────────────────────────────────────────────────

/**
 * Read SCD30 and return CO2, temperature, humidity as CSV string.
 * Also updates cached values for LED matrix display.
 * Returns "0,0,0" if data not ready.
 */
String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2      = scd30.CO2;
        lastTempC    = scd30.temperature;
        lastHumidity = scd30.relative_humidity;
        return String(lastCO2) + "," +
               String(lastTempC) + "," +
               String(lastHumidity);
    }
    return "0,0,0";
}

/**
 * Read all BNO055 data and return as CSV string.
 * Order: heading, pitch, roll, gyro xyz, linear accel xyz,
 *        gravity xyz, mag xyz, accel xyz, quat wxyz,
 *        cal sys/gyro/accel/mag, temperature
 */
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

    // Cache heading, pitch, roll for LED matrix display
    lastHeading = orientationData.orientation.x;
    lastPitch   = orientationData.orientation.y;
    lastRoll    = orientationData.orientation.z;

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
           String(sys) + "," +
           String(gyro) + "," +
           String(accel) + "," +
           String(mag) + "," +
           String(temp);
}

// ── Trigger matrix scroll with current sensor values ─────────────────────────

/**
 * Build the scroll message from cached sensor values and signal
 * the matrix thread to display it. Non-blocking — returns immediately.
 */
void triggerMatrixScroll() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;

    snprintf(matrix_msg, sizeof(matrix_msg),
             " %dF %dC %dppm %d%% %ddeg ",
             (int)round(tempF),
             (int)round(lastTempC),
             (int)round(lastCO2),
             (int)round(lastHumidity),
             (int)round(lastHeading));

    k_event_set(&matrix_event, 0x001);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Bridge.begin();

    // Register sensor functions with Bridge immediately so Python side
    // doesn't fail while sensors are still initializing
    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("get_bno_data", get_bno_data);

    // Initialize matrix
    matrix.begin();

    // Initialize Zephyr event and start matrix scroll thread
    k_event_init(&matrix_event);
    k_thread_create(&matrix_thread_data, matrix_stack,
                    K_THREAD_STACK_SIZEOF(matrix_stack),
                    matrixThread, NULL, NULL, NULL,
                    MATRIX_PRIORITY, 0, K_NO_WAIT);

    // Initialize SCD30 on QWIIC bus (Wire1) — timeout after 10 seconds
    uint32_t t = millis();
    bool scd30_ok = false;
    while (millis() - t < 10000) {
        if (scd30.begin(0x61, &Wire1)) { scd30_ok = true; break; }
        delay(100);
    }

    if (scd30_ok) {
        // Wait for first valid SCD30 reading — timeout after 10 seconds
        t = millis();
        while (millis() - t < 10000 && !scd30.dataReady()) {
            delay(100);
        }
        if (scd30.dataReady()) {
            scd30.read();
            lastCO2      = scd30.CO2;
            lastTempC    = scd30.temperature;
            lastHumidity = scd30.relative_humidity;
        }
    }

    // Initialize BNO055 on QWIIC bus (Wire1) — timeout after 10 seconds
    t = millis();
    bool bno_ok = false;
    while (millis() - t < 10000) {
        if (bno.begin()) { bno_ok = true; break; }
        delay(100);
    }

    if (bno_ok) {
        bno.setExtCrystalUse(true);

        // Get initial BNO055 heading
        sensors_event_t orientationData;
        bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
        lastHeading = orientationData.orientation.x;
        lastPitch   = orientationData.orientation.y;
        lastRoll    = orientationData.orientation.z;
    }

    // Trigger first matrix scroll
    triggerMatrixScroll();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    // Update sensors and retrigger matrix scroll when new SCD30 data is ready
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2      = scd30.CO2;
        lastTempC    = scd30.temperature;
        lastHumidity = scd30.relative_humidity;

        // Update BNO055 heading with each SCD30 cycle
        sensors_event_t orientationData;
        bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
        lastHeading = orientationData.orientation.x;
        lastPitch   = orientationData.orientation.y;
        lastRoll    = orientationData.orientation.z;

        // Signal matrix thread to scroll updated values
        triggerMatrixScroll();
    }
}

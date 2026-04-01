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

// ── Sensor instances ──────────────────────────────────────────────────────────
Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);

// ── LED matrix instance ───────────────────────────────────────────────────────
ArduinoLEDMatrix matrix;

// ── Last known sensor values for LED matrix display ──────────────────────────
float lastCO2 = 0;
float lastTempC = 0;
float lastHumidity = 0;
float lastHeading = 0;
float lastPitch = 0;
float lastRoll = 0;

// ── Animation buffer for non-blocking LED matrix scroll ──────────────────────
// Named anim so macros generate anim_buf and anim_buf_used correctly
static uint32_t anim_buf[1024][4];
static uint32_t anim_buf_used = 0;

// ── Sensor data functions ─────────────────────────────────────────────────────

/**
 * Read SCD30 and return CO2, temperature, humidity as CSV string.
 * Also updates cached values for LED matrix display.
 * Returns "0,0,0" if data not ready.
 */
String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2 = scd30.CO2;
        lastTempC = scd30.temperature;
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

// ── LED Matrix display ────────────────────────────────────────────────────────

/**
 * Pre-render scroll animation to buffer using endTextAnimation macro.
 * Non-blocking — frames are pre-computed then played back via next().
 * Sequence: Temperature F -> Temperature C -> CO2 ppm -> Humidity % -> Heading
 */
void buildMatrixAnimation() {
    float tempF = (lastTempC * 9.0 / 5.0) + 32.0;

    String msg = String((int)round(tempF)) + "F " +
                 String((int)round(lastTempC)) + "C " +
                 String((int)round(lastCO2)) + "ppm " +
                 String((int)round(lastHumidity)) + "% " +
                 String((int)round(lastHeading)) + "deg ";

    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    matrix.textScrollSpeed(50);
    matrix.textFont(Font_5x7);
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.println(msg);
    endTextAnimation(SCROLL_LEFT, anim);
    matrix.endDraw();

    // Load pre-rendered frames and loop continuously
    loadTextAnimationSequence(anim);
    matrix.autoscroll(50);
    matrix.playSequence(true);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Bridge.begin();
    matrix.begin();

    // Initialize SCD30 on QWIIC bus (Wire1)
    while (!scd30.begin(0x61, &Wire1)) {
        delay(100);
    }

    // Wait for first valid SCD30 reading before building matrix animation
    while (!scd30.dataReady()) {
        delay(100);
    }
    scd30.read();
    lastCO2 = scd30.CO2;
    lastTempC = scd30.temperature;
    lastHumidity = scd30.relative_humidity;

    // Initialize BNO055 on QWIIC bus (Wire1), use external crystal
    while (!bno.begin()) {
        delay(100);
    }
    bno.setExtCrystalUse(true);

    // Get initial BNO055 heading
    sensors_event_t orientationData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    lastHeading = orientationData.orientation.x;
    lastPitch   = orientationData.orientation.y;
    lastRoll    = orientationData.orientation.z;

    // Build initial matrix animation from first sensor readings
    buildMatrixAnimation();

    // Register sensor functions with Bridge
    Bridge.provide("get_scd_data", get_scd_data);
    Bridge.provide("get_bno_data", get_bno_data);
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    // Update SCD30 cache and rebuild animation when new data is ready
    if (scd30.dataReady()) {
        scd30.read();
        lastCO2 = scd30.CO2;
        lastTempC = scd30.temperature;
        lastHumidity = scd30.relative_humidity;

        // Also update BNO055 heading with each SCD30 reading cycle
        sensors_event_t orientationData;
        bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
        lastHeading = orientationData.orientation.x;
        lastPitch   = orientationData.orientation.y;
        lastRoll    = orientationData.orientation.z;

        // Rebuild animation with updated values
        buildMatrixAnimation();
    }
}

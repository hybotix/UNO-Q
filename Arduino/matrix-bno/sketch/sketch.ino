/**
 * Matrix BNO Sketch
 * Hybrid RobotiX
 *
 * MCU provides sensor data, accepts display content from Python.
 * - SCD30:   CO2 only
 * - SHT45:   Temperature and humidity
 * - BNO055:  Heading, pitch, roll
 * - TCA9548A: SparkFun Qwiic Mux Breakout 8-Channel
 *   - Ch 0: SparkFun VL53L5CX 8x8 ToF (zoned depth map)
 *   - Ch 1: VL53L1X long range front
 *   - Ch 2: VL53L1X long range rear
 *   - Ch 3: VL53L1X long range left
 *   - Ch 4: VL53L1X long range right
 *
 * Bridge provides:
 *   get_scd_data()         - SCD30 CO2 CSV
 *   get_sht45_data()       - SHT45 temperature and humidity CSV
 *   get_bno_data()         - BNO055 orientation CSV
 *   get_mux_data()         - All active mux channel readings as CSV
 *   set_matrix_msg(String) - Display string from Python
 */

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_SHT4x.h>
#include <utility/imumaths.h>
#include <Wire.h>
//#include <SparkFun_I2C_Mux_Arduino_Library.h>  // Activate when mux is in use

// ── TCA9548A Mux configuration ────────────────────────────────────────────────
#define MUX_ADDR            0x70  // TCA9548A default I2C address
#define MUX_CH_VL53L5CX     0     // SparkFun VL53L5CX 8x8 ToF (zoned depth map)
#define MUX_CH_VL53L1X_FRONT 1    // VL53L1X long range front
#define MUX_CH_VL53L1X_REAR  2    // VL53L1X long range rear
#define MUX_CH_VL53L1X_LEFT  3    // VL53L1X long range left
#define MUX_CH_VL53L1X_RIGHT 4    // VL53L1X long range right
#define MUX_NUM_CHANNELS    5     // Number of active channels

// Mux channel descriptor
struct MuxChannel {
    uint8_t     channel;
    const char* name;
    bool        active;
};

// Channel array — set active=true when sensor is connected and ready
MuxChannel mux_channels[MUX_NUM_CHANNELS] = {
    { MUX_CH_VL53L5CX,      "VL53L5CX",  false },
    { MUX_CH_VL53L1X_FRONT, "FRONT",     false },
    { MUX_CH_VL53L1X_REAR,  "REAR",      false },
    { MUX_CH_VL53L1X_LEFT,  "LEFT",      false },
    { MUX_CH_VL53L1X_RIGHT, "RIGHT",     false },
};

// ── Sensor instances ──────────────────────────────────────────────────────────
Arduino_LED_Matrix matrix;
Adafruit_SCD30 scd30;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
Adafruit_SHT4x sht45;
//QWIICMUX mux;  // TCA9548A at MUX_ADDR

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

String get_sht45_data() {
    sensors_event_t humidity_event, temp_event;
    sht45.getEvent(&humidity_event, &temp_event);
    return String(temp_event.temperature) + "," + String(humidity_event.relative_humidity);
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

/**
 * Cycle through all active mux channels and return readings as CSV.
 * Format: channel_name:value,channel_name:value,...
 * Returns "none" if no active channels.
 */
String get_mux_data() {
    String result = "";
    bool any = false;

    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (!mux_channels[i].active) continue;

        // Activate this mux channel
        //mux.setPort(mux_channels[i].channel);

        // Read sensor on this channel — add sensor-specific read logic here
        // Example placeholder:
        // float reading = readSensorOnChannel(mux_channels[i].channel);
        // result += String(mux_channels[i].name) + ":" + String(reading);

        result += String(mux_channels[i].name) + ":0";  // placeholder
        if (i < MUX_NUM_CHANNELS - 1) result += ",";
        any = true;
    }

    // Disable all mux channels when done
    //mux.setPort(255);

    return any ? result : "none";
}

/**
 * Enable or disable a mux channel by index.
 * Python calls: set_mux_channel("0,true") or set_mux_channel("2,false")
 */
void set_mux_channel(String params) {
    int comma = params.indexOf(',');
    if (comma < 0) return;
    int channel = params.substring(0, comma).toInt();
    bool active = params.substring(comma + 1).equalsIgnoreCase("true");
    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            mux_channels[i].active = active;
            return;
        }
    }
}

/**
 * Return all mux channels and their active state.
 * Format: channel:name:active,channel:name:active,...
 */
String get_mux_channels() {
    String result = "";
    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        result += String(mux_channels[i].channel) + ":" +
                  String(mux_channels[i].name) + ":" +
                  String(mux_channels[i].active ? "true" : "false");
        if (i < MUX_NUM_CHANNELS - 1) result += ",";
    }
    return result;
}

/**
 * Read data from a specific mux channel by channel number.
 * Returns "inactive" if channel is not active.
 * Add sensor-specific read logic per channel as sensors are connected.
 */
String get_mux_channel_data(String param) {
    int channel = param.toInt();
    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            if (!mux_channels[i].active) return "inactive";
            //mux.setPort(channel);
            // Add sensor-specific read logic here per channel
            //mux.setPort(255);
            return "0";  // placeholder
        }
    }
    return "invalid";
}


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
    sht45.begin(&Wire1);
    //mux.begin(MUX_ADDR, Wire1);  // Activate when mux is in use

    Bridge.provide("get_scd_data",          get_scd_data);
    Bridge.provide("get_sht45_data",        get_sht45_data);
    Bridge.provide("get_bno_data",          get_bno_data);
    Bridge.provide("get_mux_data",          get_mux_data);
    Bridge.provide("get_mux_channels",      get_mux_channels);
    Bridge.provide("get_mux_channel_data",  get_mux_channel_data);
    Bridge.provide("set_mux_channel",       set_mux_channel);
    Bridge.provide("set_matrix_msg",        set_matrix_msg);
    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

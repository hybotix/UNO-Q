/**
 * Matrix BNO Mux Sketch
 * Hybrid RobotiX
 *
 * Full dual-mux sensor platform using two TCA9548A mux breakouts.
 * Python is the controller — reads all sensors, formats display
 * messages, and sends them back to the MCU for display.
 *
 * Mux 1 (MUX1_ADDR 0x70) — Distance sensors:
 *   Ch 0: SparkFun VL53L5CX — 8x8 ToF zoned depth map
 *   Ch 1: VL53L1X — long range front distance
 *   Ch 2: VL53L1X — long range rear distance
 *   Ch 3: VL53L1X — long range left distance
 *   Ch 4: VL53L1X — long range right distance
 *
 * Mux 2 (MUX2_ADDR 0x71) — Environmental sensors:
 *   Ch 0: SCD30  — CO2, temperature, humidity
 *   Ch 1: SHT45  — Temperature, humidity (primary temp/humidity source)
 *   Ch 2: BNO055 — 9-DoF orientation
 *   Ch 3: BME688 — Temperature, humidity, pressure, VOC (planned)
 *   Ch 4: ENS161 — TVOC, eCO2, AQI (planned)
 *   Ch 5: AS7343   — 14-channel spectral/color sensor (planned)
 *   Ch 6: APDS9999 — proximity, lux, RGB color (planned)
 *
 * Bridge functions exposed to Python:
 *   get_scd_data()              - Read SCD30: returns "co2,tempC,humidity"
 *   get_sht45_data()            - Read SHT45: returns "tempC,humidity"
 *   get_bno_data()              - Read BNO055: returns full 27-field CSV
 *   get_as7343_data()           - Read AS7343: returns 14 spectral channel counts CSV (planned)
 *   get_mux1_data()             - Read all active mux1 channels: returns "name:value,..." or "none"
 *   get_mux2_data()             - Read all active mux2 channels: returns "name:value,..." or "none"
 *   get_mux1_channels()         - List mux1 channels: returns "ch:name:active,..."
 *   get_mux2_channels()         - List mux2 channels: returns "ch:name:active,..."
 *   get_mux1_channel_data(ch)   - Read one mux1 channel: returns value or "inactive"/"invalid"
 *   get_mux2_channel_data(ch)   - Read one mux2 channel: returns value or "inactive"/"invalid"
 *   set_mux1_channel(ch,active) - Enable/disable a mux1 channel: params "ch,true|false"
 *   set_mux2_channel(ch,active) - Enable/disable a mux2 channel: params "ch,true|false"
 *   set_matrix_msg(msg)         - Set scroll message: Python sends formatted string to display
 */

// ── Mux 1 configuration (0x70) — Distance sensors ────────────────────────────
#define MUX1_ADDR             0x70  // TCA9548A address — A0/A1/A2 all LOW
#define MUX1_CH_VL53L5CX      0     // SparkFun VL53L5CX 8x8 ToF (zoned depth map)
#define MUX1_CH_VL53L1X_FRONT 1     // VL53L1X long range distance — front
#define MUX1_CH_VL53L1X_REAR  2     // VL53L1X long range distance — rear
#define MUX1_CH_VL53L1X_LEFT  3     // VL53L1X long range distance — left
#define MUX1_CH_VL53L1X_RIGHT 4     // VL53L1X long range distance — right
#define MUX1_NUM_CHANNELS     5     // Total defined channels on mux1

// ── Mux 2 configuration (0x71) — Environmental sensors ───────────────────────
#define MUX2_ADDR             0x71  // TCA9548A address — A0 HIGH, A1/A2 LOW
#define MUX2_CH_SCD30         0     // SCD30 — CO2, temperature, humidity
#define MUX2_CH_SHT45         1     // SHT45 — temperature, humidity (primary)
#define MUX2_CH_BNO055        2     // BNO055 — 9-DoF orientation
#define MUX2_CH_BME688        3     // BME688 — temp, humidity, pressure, VOC (planned)
#define MUX2_CH_ENS161        4     // ENS161 — TVOC, eCO2, AQI (planned)
#define MUX2_CH_AS7343        5     // AS7343 — 14-channel spectral/color sensor (planned)
#define MUX2_CH_APDS9999      6     // APDS9999 — proximity, lux, RGB color (planned)
#define MUX2_NUM_CHANNELS     7     // Total defined channels on mux2

// ── Scroll configuration ──────────────────────────────────────────────────────
#define SCROLL_SPEED_MS  125  // ms per pixel — 125ms is the sweet spot for readability
#define CHAR_WIDTH         6  // Font_5x7 character width including 1px spacing

// ── Includes ──────────────────────────────────────────────────────────────────
#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_SHT4x.h>
//#include <Adafruit_VEML7700.h>       // Replaced by AS7343
#include <Adafruit_AS7343.h>
#include <Adafruit_APDS9999.h>
#include <utility/imumaths.h>
#include <Wire.h>
#include <SparkFun_I2C_Mux_Arduino_Library.h>

// ── Mux channel descriptor ────────────────────────────────────────────────────
struct MuxChannel {
    uint8_t     channel;  // TCA9548A channel number (0-7)
    const char* name;     // Human-readable name for Bridge CSV responses
    bool        active;   // true = sensor connected and ready to read
};

// Mux 1 channel array — distance sensors
// Set active=true per channel when sensor is physically connected
MuxChannel mux1_channels[MUX1_NUM_CHANNELS] = {
    { MUX1_CH_VL53L5CX,      "VL53L5CX", false },
    { MUX1_CH_VL53L1X_FRONT, "FRONT",    false },
    { MUX1_CH_VL53L1X_REAR,  "REAR",     false },
    { MUX1_CH_VL53L1X_LEFT,  "LEFT",     false },
    { MUX1_CH_VL53L1X_RIGHT, "RIGHT",    false },
};

// Mux 2 channel array — environmental sensors
// Set active=true per channel when sensor is physically connected
MuxChannel mux2_channels[MUX2_NUM_CHANNELS] = {
    { MUX2_CH_SCD30,  "SCD30",  false },
    { MUX2_CH_SHT45,  "SHT45",  false },
    { MUX2_CH_BNO055, "BNO055", false },
    { MUX2_CH_BME688,  "BME688",  false },
    { MUX2_CH_ENS161,  "ENS161",  false },
    { MUX2_CH_AS7343,  "AS7343",  false },
    { MUX2_CH_APDS9999, "APDS9999", false },
};

// ── Sensor instances ──────────────────────────────────────────────────────────
Arduino_LED_Matrix matrix;
Adafruit_SCD30     scd30;
Adafruit_BNO055    bno = Adafruit_BNO055(55, 0x28, &Wire1);
Adafruit_SHT4x     sht45;
//Adafruit_VEML7700  veml7700;         // Replaced by AS7343
Adafruit_AS7343    as7343;
Adafruit_APDS9999  apds9999;
QWIICMUX           mux1;
QWIICMUX           mux2;

// ── Scroll state machine ──────────────────────────────────────────────────────
static char          matrix_msg[64] = " ... ";
static int           scroll_x = 12;
static int           msg_pixel_width = 0;
static unsigned long last_scroll_ms = 0;

/**
 * Recalculate scroll width after message changes.
 * Must be called any time matrix_msg is updated.
 */
void updateScrollMetrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

/**
 * Advance the scroll animation by one pixel if enough time has elapsed.
 * Call from loop() — non-blocking, uses millis() for timing.
 * Resets to right edge when message has fully scrolled off left.
 */
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

/**
 * Read SCD30 CO2, temperature, and humidity via mux2 channel 0.
 * Returns: "co2,tempC,humidity" as floats (e.g. "473.2,28.1,47.5")
 * Returns: "0,0,0" if new data is not yet ready.
 * Note: SCD30 temperature reads high due to self-heating — use SHT45 for accurate temp.
 */
String get_scd_data() {
    mux2.setPort(MUX2_CH_SCD30);
    if (scd30.dataReady()) {
        scd30.read();
        mux2.setPort(255);
        return String(scd30.CO2) + "," +
               String(scd30.temperature) + "," +
               String(scd30.relative_humidity);
    }
    mux2.setPort(255);
    return "0,0,0";
}

/**
 * Read SHT45 temperature and humidity via mux2 channel 1 (high precision mode).
 * Returns: "tempC,humidity" as floats (e.g. "23.4,48.2")
 * Primary source for temperature and humidity — more accurate than SCD30.
 */
String get_sht45_data() {
    mux2.setPort(MUX2_CH_SHT45);
    sensors_event_t humidity_event, temp_event;
    sht45.getEvent(&humidity_event, &temp_event);
    mux2.setPort(255);
    return String(temp_event.temperature) + "," +
           String(humidity_event.relative_humidity);
}

/**
 * Read full BNO055 9-DoF IMU data via mux2 channel 2.
 * Returns: 27-field CSV string in this order:
 *   [0]  heading (deg)        [1]  pitch (deg)         [2]  roll (deg)
 *   [3]  gyro_x (rad/s)       [4]  gyro_y (rad/s)      [5]  gyro_z (rad/s)
 *   [6]  linaccel_x (m/s²)    [7]  linaccel_y (m/s²)   [8]  linaccel_z (m/s²)
 *   [9]  gravity_x (m/s²)     [10] gravity_y (m/s²)    [11] gravity_z (m/s²)
 *   [12] mag_x (uT)           [13] mag_y (uT)           [14] mag_z (uT)
 *   [15] accel_x (m/s²)       [16] accel_y (m/s²)      [17] accel_z (m/s²)
 *   [18] quat_w               [19] quat_x               [20] quat_y    [21] quat_z
 *   [22] cal_sys              [23] cal_gyro             [24] cal_accel [25] cal_mag
 *   [26] temperature (C)
 */
String get_bno_data() {
    mux2.setPort(MUX2_CH_BNO055);
    sensors_event_t orientationData, angVelocityData, linearAccelData, gravityData, magData, accelData;
    bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&angVelocityData, Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gravityData,     Adafruit_BNO055::VECTOR_GRAVITY);
    bno.getEvent(&magData,         Adafruit_BNO055::VECTOR_MAGNETOMETER);
    bno.getEvent(&accelData,       Adafruit_BNO055::VECTOR_ACCELEROMETER);
    imu::Quaternion quat = bno.getQuat();
    uint8_t sys, gyro, accel, mag;
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    int8_t temp = bno.getTemp();
    mux2.setPort(255);

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
           String(accel) + "," + String(mag) + "," +
           String(temp);
}

/**
 * Helper — read all active channels on a given mux.
 * Activates each channel, reads the sensor, then deactivates.
 * Returns: "name:value,..." for all active channels, or "none".
 */
String readMuxChannels(QWIICMUX& mux, MuxChannel* channels, int count) {
    String result = "";
    bool   any    = false;

    for (int i = 0; i < count; i++) {
        if (!channels[i].active) continue;
        mux.setPort(channels[i].channel);
        // TODO: Add sensor-specific read logic here per channel
        result += String(channels[i].name) + ":0";  // placeholder
        if (i < count - 1) result += ",";
        any = true;
    }

    mux.setPort(255);
    return any ? result : "none";
}

/**
 * Read all currently active mux1 channels (distance sensors).
 * Returns: "name:value,..." or "none" if no active channels.
 */
String get_mux1_data() {
    return readMuxChannels(mux1, mux1_channels, MUX1_NUM_CHANNELS);
}

/**
 * Read all currently active mux2 channels (environmental sensors).
 * Returns: "name:value,..." or "none" if no active channels.
 */
String get_mux2_data() {
    return readMuxChannels(mux2, mux2_channels, MUX2_NUM_CHANNELS);
}

/**
 * Helper — return status of all channels in a mux channel array.
 * Returns: "channel:name:active,..." for all channels.
 */
String getMuxChannels(MuxChannel* channels, int count) {
    String result = "";
    for (int i = 0; i < count; i++) {
        result += String(channels[i].channel) + ":" +
                  String(channels[i].name) + ":" +
                  String(channels[i].active ? "true" : "false");
        if (i < count - 1) result += ",";
    }
    return result;
}

/**
 * Return status of all mux1 channels (distance sensors).
 * Returns: "channel:name:active,..." for all MUX1_NUM_CHANNELS channels.
 * Example: "0:VL53L5CX:false,1:FRONT:true,..."
 */
String get_mux1_channels() {
    return getMuxChannels(mux1_channels, MUX1_NUM_CHANNELS);
}

/**
 * Return status of all mux2 channels (environmental sensors).
 * Returns: "channel:name:active,..." for all MUX2_NUM_CHANNELS channels.
 * Example: "0:SCD30:true,1:SHT45:true,..."
 */
String get_mux2_channels() {
    return getMuxChannels(mux2_channels, MUX2_NUM_CHANNELS);
}

/**
 * Helper — read a single channel from a mux.
 * Returns: sensor reading, "inactive", or "invalid".
 */
String getMuxChannelData(QWIICMUX& mux, MuxChannel* channels, int count, int channel) {
    for (int i = 0; i < count; i++) {
        if (channels[i].channel == channel) {
            if (!channels[i].active) return "inactive";
            mux.setPort(channel);
            // TODO: Add sensor-specific read logic here per channel
            mux.setPort(255);
            return "0";  // placeholder
        }
    }
    return "invalid";
}

/**
 * Read data from a single mux1 channel (distance sensors).
 * Parameter: channel number as string (e.g. "0", "1")
 * Returns: sensor reading, "inactive" if not enabled, "invalid" if not defined.
 */
String get_mux1_channel_data(String param) {
    return getMuxChannelData(mux1, mux1_channels, MUX1_NUM_CHANNELS, param.toInt());
}

/**
 * Read data from a single mux2 channel (environmental sensors).
 * Parameter: channel number as string (e.g. "0", "1")
 * Returns: sensor reading, "inactive" if not enabled, "invalid" if not defined.
 */
String get_mux2_channel_data(String param) {
    return getMuxChannelData(mux2, mux2_channels, MUX2_NUM_CHANNELS, param.toInt());
}

/**
 * Helper — enable or disable a channel in a mux channel array.
 * Parameter: "channel,state" where state is "true" or "false".
 */
void setMuxChannel(MuxChannel* channels, int count, String params) {
    int comma = params.indexOf(',');
    if (comma < 0) return;
    int  channel = params.substring(0, comma).toInt();
    bool active  = params.substring(comma + 1).equalsIgnoreCase("true");
    for (int i = 0; i < count; i++) {
        if (channels[i].channel == channel) {
            channels[i].active = active;
            return;
        }
    }
}

/**
 * Enable or disable a mux1 channel (distance sensors).
 * Parameter: "channel,state" — e.g. "0,true" or "2,false"
 * Example: Bridge.call("set_mux1_channel", "1,true") — enable front VL53L1X
 */
void set_mux1_channel(String params) {
    setMuxChannel(mux1_channels, MUX1_NUM_CHANNELS, params);
}

/**
 * Enable or disable a mux2 channel (environmental sensors).
 * Parameter: "channel,state" — e.g. "0,true" or "3,false"
 * Example: Bridge.call("set_mux2_channel", "0,true") — enable SCD30
 */
void set_mux2_channel(String params) {
    setMuxChannel(mux2_channels, MUX2_NUM_CHANNELS, params);
}

/**
 * Set the LED matrix scroll message.
 * Parameter: formatted string to display (e.g. " 72°F(22°C) 47% 473ppm ")
 * Clears the matrix, resets scroll position, and begins scrolling the new message.
 * Python calls this after formatting sensor data into a human-readable string.
 * Scroll speed is controlled by SCROLL_SPEED_MS (currently 125ms per pixel).
 */
void set_matrix_msg(String msg) {
    matrix.clear();
    msg.toCharArray(matrix_msg, sizeof(matrix_msg));
    updateScrollMetrics();
    scroll_x = 12;
}

/**
 * Calibrate SCD30 temperature offset using SHT45 as reference.
 * Averages 5 samples from both sensors, calculates the difference,
 * and applies it as a temperature offset to the SCD30.
 * Offset is stored in SCD30 non-volatile memory — persists across power cycles.
 * Also improves CO2 accuracy since SCD30's internal compensation uses temperature.
 * Only applies offset if between 0.5°C and 20°C (sanity bounds).
 * Returns: "offset:X.XX" on success, "skipped" if offset out of bounds, "error" on failure.
 * Python calls this once at startup if ~/.scd30-calibrated does not exist.
 */
String calibrate_scd30() {
    float scd30_temp_sum = 0;
    float sht45_temp_sum = 0;
    int   samples        = 0;

    while (samples < 5) {
        mux2.setPort(MUX2_CH_SCD30);
        while (!scd30.dataReady()) { delay(100); }
        scd30.read();

        mux2.setPort(MUX2_CH_SHT45);
        sensors_event_t humidity_event, temp_event;
        sht45.getEvent(&humidity_event, &temp_event);

        mux2.setPort(255);

        scd30_temp_sum += scd30.temperature;
        sht45_temp_sum += temp_event.temperature;
        samples++;
        delay(500);
    }

    float scd30_avg = scd30_temp_sum / samples;
    float sht45_avg = sht45_temp_sum / samples;
    float offset    = scd30_avg - sht45_avg;

    if (offset > 0.5 && offset < 20.0) {
        mux2.setPort(MUX2_CH_SCD30);
        scd30.setTemperatureOffset(offset);
        mux2.setPort(255);
        return "offset:" + String(offset, 2);
    }

    return "skipped";
}

/**
 * Read AS7343 14-channel spectral/color sensor via mux2 channel 5.
 * Returns: "ch0,ch1,...,ch13" — 14 raw spectral channel counts
 * Channels span 400nm–1000nm visible and NIR spectrum.
 */
String get_as7343_data() {
    mux2.setPort(MUX2_CH_AS7343);
    uint16_t readings[14];
    as7343.readAllChannels(readings);
    mux2.setPort(255);
    String result = "";
    for (int i = 0; i < 14; i++) {
        result += String(readings[i]);
        if (i < 13) result += ",";
    }
    return result;
}

/**
 * Read APDS9999 proximity, lux, and RGB color sensor via mux2 channel 6.
 * Returns: "proximity,lux,r,g,b,ir" as integers
 *   proximity — IR proximity (0-65535, higher = closer)
 *   lux       — calculated lux value
 *   r,g,b,ir  — raw red, green, blue, infrared channel counts
 */
String get_apds9999_data() {
    mux2.setPort(MUX2_CH_APDS9999);
    uint16_t r, g, b, ir;
    apds9999.getRGBIR(&r, &g, &b, &ir);
    uint16_t proximity = apds9999.getProximity();
    float    lux       = apds9999.calculateLux(g);
    mux2.setPort(255);
    return String(proximity) + "," +
           String(lux, 2) + "," +
           String(r) + "," +
           String(g) + "," +
           String(b) + "," +
           String(ir);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();

    // Init mux1 (distance sensors) and mux2 (environmental sensors)
    mux1.begin(MUX1_ADDR, Wire1);
    mux2.begin(MUX2_ADDR, Wire1);

    // Init environmental sensors via mux2
    mux2.setPort(MUX2_CH_SCD30);
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }

    mux2.setPort(MUX2_CH_BNO055);
    while (!bno.begin()) { delay(100); }
    bno.setExtCrystalUse(true);

    mux2.setPort(MUX2_CH_SHT45);
    sht45.begin(&Wire1);

    //mux2.setPort(MUX2_CH_AS7343);
    //as7343.begin(&Wire1);  // Uncomment when AS7343 is connected

    //mux2.setPort(MUX2_CH_APDS9999);
    //apds9999.begin(&Wire1);  // Uncomment when APDS9999 is connected

    mux2.setPort(255);  // Disable all mux2 channels

    Bridge.provide("get_scd_data",          get_scd_data);
    Bridge.provide("get_sht45_data",        get_sht45_data);
    Bridge.provide("get_bno_data",          get_bno_data);
    Bridge.provide("get_as7343_data",     get_as7343_data);
    Bridge.provide("get_apds9999_data",   get_apds9999_data);
    Bridge.provide("get_mux1_data",         get_mux1_data);
    Bridge.provide("get_mux2_data",         get_mux2_data);
    Bridge.provide("get_mux1_channels",     get_mux1_channels);
    Bridge.provide("get_mux2_channels",     get_mux2_channels);
    Bridge.provide("get_mux1_channel_data", get_mux1_channel_data);
    Bridge.provide("get_mux2_channel_data", get_mux2_channel_data);
    Bridge.provide("set_mux1_channel",      set_mux1_channel);
    Bridge.provide("set_mux2_channel",      set_mux2_channel);
    Bridge.provide("calibrate_scd30",       calibrate_scd30);
    Bridge.provide("set_matrix_msg",        set_matrix_msg);
    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

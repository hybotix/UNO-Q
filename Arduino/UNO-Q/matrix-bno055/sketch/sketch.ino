/**
 * Matrix BNO Sketch
 * Hybrid RobotiX
 *
 * MCU provides sensor data and accepts display content from Python.
 * Python is the controller — it reads all sensors, formats display
 * messages, and sends them back to the MCU for display.
 *
 * Sensors (direct QWIIC — no mux):
 *   - SCD30:    CO2 (ppm), temperature (C), humidity (%)
 *   - SHT45:    Temperature (C), humidity (%) — primary temp/humidity source
 *   - BNO055:   9-DoF orientation — heading, pitch, roll + full IMU data
 *   - AS7343:   14-channel spectral/color sensor
 *   - APDS9999: Proximity, lux, RGB color
 *   - SGP41:    VOC & NOx gas sensor
 *
 * Mux 1 (MUX_ADDR 0x70) — Distance sensors (pending hardware):
 *   Ch 0: SparkFun VL53L5CX — 8x8 ToF zoned depth map
 *   Ch 1: VL53L1X — long range front distance
 *   Ch 2: VL53L1X — long range rear distance
 *   Ch 3: VL53L1X — long range left distance
 *   Ch 4: VL53L1X — long range right distance
 *
 * Bridge functions exposed to Python:
 *   get_scd30_data()              - Read SCD30: returns "co2,tempC,humidity"
 *   get_sht45_data()            - Read SHT45: returns "tempC,humidity"
 *   get_bno055_data()              - Read BNO055: returns full 27-field CSV
 *   get_as7343_data()           - Read AS7343: returns 14 spectral channel counts CSV
 *   get_apds9999_data()         - Read APDS9999: returns "proximity,lux,r,g,b,ir"
 *   get_sgp41_data()            - Read SGP41: returns "voc_raw,nox_raw"
 *   get_mux_data()              - Read all active mux channels: returns "name:value,..." or "none"
 *   get_mux_channels()          - List all channels: returns "ch:name:active,..."
 *   get_mux_channel_data(ch)    - Read one mux channel: returns value or "inactive"/"invalid"
 *   set_mux_channel(ch,active)  - Enable/disable a mux channel: params "ch,true|false"
 *   calibrate_scd30()           - Calibrate SCD30 temp offset using SHT45: returns "offset:X.XX"
 *   set_matrix_msg(msg)         - Set scroll message: Python sends formatted string to display
 */

// ── TCA9548A Mux configuration ────────────────────────────────────────────────
#define MUX_ADDR             0x70  // TCA9548A default I2C address on Wire1
#define MUX_CH_VL53L5CX      0     // SparkFun VL53L5CX 8x8 ToF (zoned depth map)
#define MUX_CH_VL53L1X_FRONT 1     // VL53L1X long range distance — front
#define MUX_CH_VL53L1X_REAR  2     // VL53L1X long range distance — rear
#define MUX_CH_VL53L1X_LEFT  3     // VL53L1X long range distance — left
#define MUX_CH_VL53L1X_RIGHT 4     // VL53L1X long range distance — right
#define MUX_NUM_CHANNELS     5     // Total number of defined mux channels

// ── Scroll configuration ──────────────────────────────────────────────────────
#define SCROLL_SPEED_MS  125  // ms per pixel — 125ms is the sweet spot for readability
#define CHAR_WIDTH         6  // Font_5x7 character width including 1px spacing
#define SCROLLING_ENABLED true  // Set to false to disable matrix scrolling in production

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
#include <Adafruit_SGP41.h>
#include <utility/imumaths.h>
#include <Wire.h>
//#include <SparkFun_I2C_Mux_Arduino_Library.h>  // Uncomment when mux is in use

// ── Mux channel descriptor ────────────────────────────────────────────────────
struct MuxChannel {
    uint8_t     channel;  // TCA9548A channel number (0-7)
    const char* name;     // Human-readable name for Bridge CSV responses
    bool        active;   // true = sensor connected and ready to read
};

// Channel array — set active=true per channel when sensor is physically connected
MuxChannel mux_channels[MUX_NUM_CHANNELS] = {
    { MUX_CH_VL53L5CX,      "VL53L5CX", false },
    { MUX_CH_VL53L1X_FRONT, "FRONT",    false },
    { MUX_CH_VL53L1X_REAR,  "REAR",     false },
    { MUX_CH_VL53L1X_LEFT,  "LEFT",     false },
    { MUX_CH_VL53L1X_RIGHT, "RIGHT",    false },
};

// ── Sensor instances ──────────────────────────────────────────────────────────
Arduino_LED_Matrix matrix;
Adafruit_SCD30     scd30;
Adafruit_BNO055    bno = Adafruit_BNO055(55, 0x28, &Wire1);
Adafruit_SHT4x     sht45;
//Adafruit_VEML7700  veml7700;         // Replaced by AS7343
Adafruit_AS7343    as7343;
Adafruit_APDS9999  apds9999;
Adafruit_SGP41     sgp41;
//QWIICMUX mux;  // Uncomment when TCA9548A is in use

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
 * Has no effect if SCROLLING_ENABLED is false.
 */
void scrollTick() {
    if (!SCROLLING_ENABLED) return;
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
 * Read SCD30 CO2, temperature, and humidity.
 * Returns: "co2,tempC,humidity" as floats (e.g. "473.2,28.1,47.5")
 * Returns: "0,0,0" if new data is not yet ready.
 * Note: SCD30 temperature reads high due to self-heating — use SHT45 for accurate temp.
 */
String get_scd30_data() {
    if (scd30.dataReady()) {
        scd30.read();
        return String(scd30.CO2) + "," +
               String(scd30.temperature) + "," +
               String(scd30.relative_humidity);
    }
    return "0,0,0";
}

/**
 * Read SHT45 temperature and humidity (high precision mode).
 * Returns: "tempC,humidity" as floats (e.g. "23.4,48.2")
 * Primary source for temperature and humidity — more accurate than SCD30.
 */
String get_sht45_data() {
    sensors_event_t humidity_event, temp_event;
    sht45.getEvent(&humidity_event, &temp_event);
    return String(temp_event.temperature) + "," +
           String(humidity_event.relative_humidity);
}

/**
 * Read full BNO055 9-DoF IMU data.
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
String get_bno055_data() {
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
 * Read all currently active mux channels in sequence.
 * Activates each channel, reads the sensor, then deactivates.
 * Returns: "name:value,name:value,..." for all active channels
 * Returns: "none" if no channels are currently active.
 * Add sensor-specific read logic per channel as sensors are connected.
 */
String get_mux_data() {
    String result = "";
    bool   any    = false;

    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (!mux_channels[i].active) continue;

        //mux.setPort(mux_channels[i].channel);
        // TODO: Add sensor-specific read logic here per channel
        result += String(mux_channels[i].name) + ":0";  // placeholder
        if (i < MUX_NUM_CHANNELS - 1) result += ",";
        any = true;
    }

    //mux.setPort(255);  // Disable all channels when done
    return any ? result : "none";
}

/**
 * Return status of all defined mux channels.
 * Returns: "channel:name:active,..." for all MUX_NUM_CHANNELS channels
 * Example: "0:VL53L5CX:false,1:FRONT:true,2:REAR:false,..."
 * Use to discover which channels are available and their current state.
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
 * Read data from a single specific mux channel.
 * Parameter: channel number as string (e.g. "0", "1")
 * Returns: sensor reading as string if channel is active
 * Returns: "inactive" if the channel exists but is not enabled
 * Returns: "invalid" if the channel number is not defined
 * Add sensor-specific read logic per channel as sensors are connected.
 */
String get_mux_channel_data(String param) {
    int channel = param.toInt();
    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            if (!mux_channels[i].active) return "inactive";
            //mux.setPort(channel);
            // TODO: Add sensor-specific read logic here per channel
            //mux.setPort(255);
            return "0";  // placeholder
        }
    }
    return "invalid";
}

/**
 * Enable or disable a specific mux channel.
 * Parameter: "channel,state" where channel is the channel number
 *            and state is "true" or "false" (case insensitive)
 * Example: Bridge.call("set_mux_channel", "0,true")  — enable channel 0
 * Example: Bridge.call("set_mux_channel", "2,false") — disable channel 2
 * Has no effect if channel number is not defined in mux_channels[].
 */
void set_mux_channel(String params) {
    int comma = params.indexOf(',');
    if (comma < 0) return;
    int  channel = params.substring(0, comma).toInt();
    bool active  = params.substring(comma + 1).equalsIgnoreCase("true");
    for (int i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            mux_channels[i].active = active;
            return;
        }
    }
}

/**
 * Set the LED matrix scroll message.
 * Parameter: formatted string to display (e.g. " 72°F(22°C) 47% 473ppm ")
 * Clears the matrix, resets scroll position, and begins scrolling the new message.
 * Python calls this after formatting sensor data into a human-readable string.
 * Scroll speed is controlled by SCROLL_SPEED_MS (currently 125ms per pixel).
 * Has no effect if SCROLLING_ENABLED is false.
 */
void set_matrix_msg(String msg) {
    if (!SCROLLING_ENABLED) return;
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
        while (!scd30.dataReady()) { delay(100); }
        scd30.read();

        sensors_event_t humidity_event, temp_event;
        sht45.getEvent(&humidity_event, &temp_event);

        scd30_temp_sum += scd30.temperature;
        sht45_temp_sum += temp_event.temperature;
        samples++;
        delay(500);
    }

    float scd30_avg = scd30_temp_sum / samples;
    float sht45_avg = sht45_temp_sum / samples;
    float offset    = scd30_avg - sht45_avg;

    if (offset > 0.5 && offset < 20.0) {
        scd30.setTemperatureOffset(offset);
        return "offset:" + String(offset, 2);
    }

    return "skipped";
}


/**
 * Read AS7343 14-channel spectral/color sensor.
 * Returns: "ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8,ch9,ch10,ch11,ch12,ch13" — raw channel counts
 * Channels cover 400nm–1000nm visible and NIR spectrum.
 */
String get_as7343_data() {
    uint16_t readings[14];
    as7343.readAllChannels(readings);
    String result = "";
    for (int i = 0; i < 14; i++) {
        result += String(readings[i]);
        if (i < 13) result += ",";
    }
    return result;
}


/**
 * Read APDS9999 proximity, lux, and RGB color sensor.
 * Returns: "proximity,lux,r,g,b,ir" as integers
 *   proximity — IR proximity (0-65535, higher = closer)
 *   lux       — calculated lux value
 *   r,g,b,ir  — raw red, green, blue, infrared channel counts (uint32_t)
 */
String get_apds9999_data() {
    uint32_t r, g, b, ir;
    uint16_t proximity;
    apds9999.getRGBIRData(&r, &g, &b, &ir);
    apds9999.readProximity(&proximity);
    float lux = apds9999.calculateLux(g);
    return String(proximity) + "," +
           String(lux, 2) + "," +
           String(r) + "," +
           String(g) + "," +
           String(b) + "," +
           String(ir);
}


/**
 * Read SGP41 VOC and NOx gas sensor.
 * Returns: "voc_raw,nox_raw" as integers
 *   voc_raw — raw VOC signal (0-65535)
 *   nox_raw — raw NOx signal (0-65535)
 * Use Sensirion VOC/NOx algorithm for index values.
 */
String get_sgp41_data() {
    uint16_t voc_raw, nox_raw;
    sgp41.measureRawSignals(&voc_raw, &nox_raw);
    return String(voc_raw) + "," + String(nox_raw);
}


void setup() {
    matrix.begin();
    matrix.clear();
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    while (!bno.begin())               { delay(100); }
    bno.setExtCrystalUse(true);
    sht45.begin(&Wire1);
    //as7343.begin(&Wire1);  // Uncomment when AS7343 is connected
    //apds9999.begin(&Wire1);           // Uncomment when APDS9999 is connected
    //apds9999.enableLightSensor(true);  // Uncomment when APDS9999 is connected
    //apds9999.enableProximitySensor(true); // Uncomment when APDS9999 is connected
    //apds9999.setRGBMode(true);         // Uncomment when APDS9999 is connected
    //sgp41.begin(&Wire1);     // Uncomment when SGP41 is connected
    //mux.begin(MUX_ADDR, Wire1);  // Uncomment when TCA9548A is in use

    Bridge.provide("get_scd30_data",         get_scd30_data);
    Bridge.provide("get_sht45_data",       get_sht45_data);
    Bridge.provide("get_bno055_data",         get_bno055_data);
    Bridge.provide("get_as7343_data",    get_as7343_data);
    Bridge.provide("get_apds9999_data",  get_apds9999_data);
    Bridge.provide("get_sgp41_data",     get_sgp41_data);
    Bridge.provide("get_mux_data",         get_mux_data);
    Bridge.provide("get_mux_channels",     get_mux_channels);
    Bridge.provide("get_mux_channel_data", get_mux_channel_data);
    Bridge.provide("set_mux_channel",      set_mux_channel);
    Bridge.provide("calibrate_scd30",      calibrate_scd30);
    Bridge.provide("set_matrix_msg",       set_matrix_msg);
    updateScrollMetrics();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    scrollTick();
}

/**
 * hub5-bno055 Sketch
 * Hybrid RobotiX
 *
 * MCU provides sensor data to Python via RouterBridge.
 * Python reads all sensors, formats display content, and sends
 * it back to the MCU for display on a 64x32 HUB75 RGB LED matrix panel.
 *
 * Cloned from matrix-bno055.
 * Arduino_LED_Matrix will be replaced by HUB75nano when display layer is implemented.
 *
 * Sensors (direct QWIIC — no mux):
 *   - SCD41:    CO2 (ppm), temperature (C), humidity (%)
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
 *   get_scd41_data()            - Read SCD41: returns "co2,temp_c,humidity"
 *   get_sht45_data()            - Read SHT45: returns "temp_c,humidity"
 *   get_bno055_data()           - Read BNO055: returns full 27-field CSV
 *   get_as7343_data()           - Read AS7343: returns 14 spectral channel counts CSV
 *   get_apds9999_data()         - Read APDS9999: returns "proximity,lux,r,g,b,ir"
 *   get_sgp41_data()            - Read SGP41: returns "voc_raw,nox_raw"
 *   get_mux_data()              - Read all active mux channels: returns "name:value,..." or "none"
 *   get_mux_channels()          - List all channels: returns "ch:name:active,..."
 *   get_mux_channel_data(ch)    - Read one mux channel: returns value or "inactive"/"invalid"
 *   set_mux_channel(ch,active)  - Enable/disable a mux channel: params "ch,true|false"
 *   set_matrix_msg(msg)         - Set display message
 */

#define MUX_ADDR             0x70
#define MUX_CH_VL53L5CX      0
#define MUX_CH_VL53L1X_FRONT 1
#define MUX_CH_VL53L1X_REAR  2
#define MUX_CH_VL53L1X_LEFT  3
#define MUX_CH_VL53L1X_RIGHT 4
#define MUX_NUM_CHANNELS     5
#define SCROLL_SPEED_MS      125
#define CHAR_WIDTH           6
#define SCROLLING_ENABLED    true

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_AS7343.h>
#include <Adafruit_APDS9999.h>
#include <Adafruit_SGP41.h>
#include <utility/imumaths.h>
#include <Wire.h>

struct MuxChannel {
    uint8_t     channel;
    const char* name;
    bool        active;
};

MuxChannel mux_channels[MUX_NUM_CHANNELS] = {
    { MUX_CH_VL53L5CX,      "VL53L5CX", false },
    { MUX_CH_VL53L1X_FRONT, "FRONT",    false },
    { MUX_CH_VL53L1X_REAR,  "REAR",     false },
    { MUX_CH_VL53L1X_LEFT,  "LEFT",     false },
    { MUX_CH_VL53L1X_RIGHT, "RIGHT",    false },
};

Arduino_LED_Matrix matrix;
SensirionI2cScd4x scd41;
Adafruit_BNO055    bno = Adafruit_BNO055(55, 0x28, &Wire1);
Adafruit_SHT4x     sht45;
Adafruit_AS7343    as7343;
Adafruit_APDS9999  apds9999;
Adafruit_SGP41     sgp41;

static char          matrix_msg[64]  = " ... ";
static int           scroll_x        = 12;
static int           msg_pixel_width = 0;
static unsigned long last_scroll_ms  = 0;

void update_scroll_metrics() {
    msg_pixel_width = strlen(matrix_msg) * CHAR_WIDTH;
}

void scroll_tick() {
    // Scroll the LED matrix message
    if (SCROLLING_ENABLED) {
        // Throttle scroll rate to SCROLL_SPEED_MS interval
        if (millis() - last_scroll_ms < SCROLL_SPEED_MS) {
            return;
        }

        last_scroll_ms = millis();

        matrix.beginDraw();
        matrix.stroke(0xFFFFFFFF);
        matrix.textFont(Font_5x7);
        matrix.beginText(scroll_x, 1, 0xFFFFFF);
        matrix.print(matrix_msg);
        matrix.endText();
        matrix.endDraw();

        scroll_x--;

        // Message fully scrolled — reset position
        if (scroll_x < -msg_pixel_width) {
            scroll_x = 12;
        }
    }
}

String get_scd41_data() {
    uint16_t co2         = 0;
    float    temperature = 0.0;
    float    humidity    = 0.0;
    bool     data_ready  = false;
    uint16_t error;

    error = scd41.getDataReadyStatus(data_ready);

    // Check for error or invalid reading
    if (error || !data_ready) {
        return "0,0,0";
    }

    error = scd41.readMeasurement(co2, temperature, humidity);

    // Check for error or invalid reading
    if (error || co2 == 0) {
        return "0,0,0";
    }

    return String(co2) + "," + String(temperature) + "," + String(humidity);
}

String get_sht45_data() {
    sensors_event_t humidity_event, temp_event;
    sht45.getEvent(&humidity_event, &temp_event);
    return String(temp_event.temperature) + "," + String(humidity_event.relative_humidity);
}

String get_bno055_data() {
    sensors_event_t  orientation_data, ang_velocity_data, linear_accel_data, gravity_data, mag_data, accel_data;
    imu::Quaternion  quat;
    uint8_t          sys, gyro, accel, mag;
    int8_t           temp;
    bno.getEvent(&orientation_data,   Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&ang_velocity_data,  Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&linear_accel_data,  Adafruit_BNO055::VECTOR_LINEARACCEL);
    bno.getEvent(&gravity_data,       Adafruit_BNO055::VECTOR_GRAVITY);
    bno.getEvent(&mag_data,           Adafruit_BNO055::VECTOR_MAGNETOMETER);
    bno.getEvent(&accel_data,         Adafruit_BNO055::VECTOR_ACCELEROMETER);
    quat = bno.getQuat();
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    temp = bno.getTemp();
    return String(orientation_data.orientation.x) + "," + String(orientation_data.orientation.y) + "," + String(orientation_data.orientation.z) + "," + String(ang_velocity_data.gyro.x) + "," + String(ang_velocity_data.gyro.y) + "," + String(ang_velocity_data.gyro.z) + "," + String(linear_accel_data.acceleration.x) + "," + String(linear_accel_data.acceleration.y) + "," + String(linear_accel_data.acceleration.z) + "," + String(gravity_data.acceleration.x) + "," + String(gravity_data.acceleration.y) + "," + String(gravity_data.acceleration.z) + "," + String(mag_data.magnetic.x) + "," + String(mag_data.magnetic.y) + "," + String(mag_data.magnetic.z) + "," + String(accel_data.acceleration.x) + "," + String(accel_data.acceleration.y) + "," + String(accel_data.acceleration.z) + "," + String(quat.w(), 4) + "," + String(quat.x(), 4) + "," + String(quat.y(), 4) + "," + String(quat.z(), 4) + "," + String(sys) + "," + String(gyro) + "," + String(accel) + "," + String(mag) + "," + String(temp);
}

String get_mux_data() {
    String result = "";
    bool   any    = false;
    int    i;

    // Iterate over channels
    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        // Found matching channel
        if (mux_channels[i].active) {
            result += String(mux_channels[i].name) + ":0";

            // Check condition
            if (i < MUX_NUM_CHANNELS - 1) {
                result += ",";
            }

            any = true;
        }
    }

    return any ? result : "none";
}

String get_mux_channels() {
    String result = "";
    int    i;

    // Iterate over channels
    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        result += String(mux_channels[i].channel) + ":" + String(mux_channels[i].name) + ":" + String(mux_channels[i].active ? "true" : "false");

        // Check condition
        if (i < MUX_NUM_CHANNELS - 1) {
            result += ",";
        }
    }

    return result;
}

String get_mux_channel_data(String param) {
    int channel = param.toInt();
    int i;

    // Iterate over channels
    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        // Found matching channel
        if (mux_channels[i].channel == channel) {
            // Found matching channel
            if (mux_channels[i].active) {
                return "0";
            }

            return "inactive";
        }
    }

    return "invalid";
}

void set_mux_channel(String params) {
    int  comma   = params.indexOf(',');
    int  channel;
    bool active;
    int  i;

    // No comma separator — invalid params
    if (comma < 0) {
        return;
    }

    channel = params.substring(0, comma).toInt();
    active  = params.substring(comma + 1).equalsIgnoreCase("true");

    // Iterate over channels
    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        // Found matching channel
        if (mux_channels[i].channel == channel) {
            mux_channels[i].active = active;
            return;
        }
    }
}

void set_matrix_msg(String msg) {
    // Scroll the LED matrix message
    if (SCROLLING_ENABLED) {
        matrix.clear();
        msg.toCharArray(matrix_msg, sizeof(matrix_msg));
        update_scroll_metrics();
        scroll_x = 12;
    }
}

String get_as7343_data() {
    uint16_t readings[14];
    String   result = "";
    int      i;
    as7343.readAllChannels(readings);

    // Iterate over channels
    for (i = 0; i < 14; i++) {
        result += String(readings[i]);

        // Check condition
        if (i < 13) {
            result += ",";
        }
    }

    return result;
}

String get_apds9999_data() {
    uint32_t r, g, b, ir;
    uint16_t proximity;
    float    lux;
    apds9999.getRGBIRData(&r, &g, &b, &ir);
    apds9999.readProximity(&proximity);
    lux = apds9999.calculateLux(g);
    return String(proximity) + "," + String(lux, 2) + "," + String(r) + "," + String(g) + "," + String(b) + "," + String(ir);
}

String get_sgp41_data() {
    uint16_t voc_raw, nox_raw;
    sgp41.measureRawSignals(&voc_raw, &nox_raw);
    return String(voc_raw) + "," + String(nox_raw);
}

void setup() {
    matrix.begin();
    matrix.clear();

    scd41.begin(Wire1, SCD41_I2C_ADDR_62);
    scd41.startPeriodicMeasurement();

    while (!bno.begin()) {
        delay(100);
    }

    bno.setExtCrystalUse(true);
    sht45.begin(&Wire1);

    Bridge.provide("get_scd41_data",         get_scd41_data);
    Bridge.provide("get_sht45_data",         get_sht45_data);
    Bridge.provide("get_bno055_data",        get_bno055_data);
    Bridge.provide("get_as7343_data",        get_as7343_data);
    Bridge.provide("get_apds9999_data",      get_apds9999_data);
    Bridge.provide("get_sgp41_data",         get_sgp41_data);
    Bridge.provide("get_mux_data",           get_mux_data);
    Bridge.provide("get_mux_channels",       get_mux_channels);
    Bridge.provide("get_mux_channel_data",   get_mux_channel_data);
    Bridge.provide("set_mux_channel",        set_mux_channel);
    Bridge.provide("set_matrix_msg",         set_matrix_msg);
    Bridge.begin();Bridge.begin();
    update_scroll_metrics();
}

void loop() {
    scroll_tick();
}

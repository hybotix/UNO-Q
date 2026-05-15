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
 *   get_scd30_data()            - Read SCD30: returns "co2,temp_c,humidity"
 *   get_sht45_data()            - Read SHT45: returns "temp_c,humidity"
 *   get_bno055_data()           - Read BNO055: returns full 27-field CSV
 *   get_as7343_data()           - Read AS7343: returns 14 spectral channel counts CSV
 *   get_apds9999_data()         - Read APDS9999: returns "proximity,lux,r,g,b,ir"
 *   get_sgp41_data()            - Read SGP41: returns "voc_raw,nox_raw"
 *   get_mux_data()              - Read all active mux channels: returns "name:value,..." or "none"
 *   get_mux_channels()          - List all channels: returns "ch:name:active,..."
 *   get_mux_channel_data(ch)    - Read one mux channel: returns value or "inactive"/"invalid"
 *   set_mux_channel(ch,active)  - Enable/disable a mux channel: params "ch,true|false"
 *   calibrate_scd30()           - Calibrate SCD30 temp offset using SHT45
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
#include <ArduinoGraphics.h>
#include <Adafruit_SCD30.h>
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
Adafruit_SCD30     scd30;
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
    if (SCROLLING_ENABLED) {
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

        if (scroll_x < -msg_pixel_width) {
            scroll_x = 12;
        }
    }
}

String get_scd30_data() {
    if (scd30.dataReady()) {
        scd30.read();
        return String(scd30.CO2) + "," + String(scd30.temperature) + "," + String(scd30.relative_humidity);
    }

    return "0,0,0";
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

    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].active) {
            result += String(mux_channels[i].name) + ":0";

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

    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        result += String(mux_channels[i].channel) + ":" + String(mux_channels[i].name) + ":" + String(mux_channels[i].active ? "true" : "false");

        if (i < MUX_NUM_CHANNELS - 1) {
            result += ",";
        }
    }

    return result;
}

String get_mux_channel_data(String param) {
    int channel = param.toInt();
    int i;

    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            if (!mux_channels[i].active) {
                return "inactive";
            }

            return "0";
        }
    }

    return "invalid";
}

void set_mux_channel(String params) {
    int  comma   = params.indexOf(',');
    int  channel;
    bool active;
    int  i;

    if (comma < 0) {
        return;
    }

    channel = params.substring(0, comma).toInt();
    active  = params.substring(comma + 1).equalsIgnoreCase("true");

    for (i = 0; i < MUX_NUM_CHANNELS; i++) {
        if (mux_channels[i].channel == channel) {
            mux_channels[i].active = active;
            return;
        }
    }
}

void set_matrix_msg(String msg) {
    if (SCROLLING_ENABLED) {
        matrix.clear();
        msg.toCharArray(matrix_msg, sizeof(matrix_msg));
        update_scroll_metrics();
        scroll_x = 12;
    }
}

String calibrate_scd30() {
    float scd30_temp_sum = 0;
    float sht45_temp_sum = 0;
    int   samples        = 0;
    float scd30_avg;
    float sht45_avg;
    float offset;

    while (samples < 5) {
        while (!scd30.dataReady()) {
            delay(100);
        }

        scd30.read();

        sensors_event_t humidity_event, temp_event;
        sht45.getEvent(&humidity_event, &temp_event);

        scd30_temp_sum += scd30.temperature;
        sht45_temp_sum += temp_event.temperature;
        samples++;
        delay(500);
    }

    scd30_avg = scd30_temp_sum / samples;
    sht45_avg = sht45_temp_sum / samples;
    offset    = scd30_avg - sht45_avg;

    if (offset > 0.5 && offset < 20.0) {
        scd30.setTemperatureOffset(offset);
        return "offset:" + String(offset, 2);
    }

    return "skipped";
}

String get_as7343_data() {
    uint16_t readings[14];
    String   result = "";
    int      i;
    as7343.readAllChannels(readings);

    for (i = 0; i < 14; i++) {
        result += String(readings[i]);

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
    Bridge.begin();

    while (!scd30.begin(0x61, &Wire1)) {
        delay(100);
    }

    while (!bno.begin()) {
        delay(100);
    }

    bno.setExtCrystalUse(true);
    sht45.begin(&Wire1);

    Bridge.provide("get_scd30_data",         get_scd30_data);
    Bridge.provide("get_sht45_data",         get_sht45_data);
    Bridge.provide("get_bno055_data",        get_bno055_data);
    Bridge.provide("get_as7343_data",        get_as7343_data);
    Bridge.provide("get_apds9999_data",      get_apds9999_data);
    Bridge.provide("get_sgp41_data",         get_sgp41_data);
    Bridge.provide("get_mux_data",           get_mux_data);
    Bridge.provide("get_mux_channels",       get_mux_channels);
    Bridge.provide("get_mux_channel_data",   get_mux_channel_data);
    Bridge.provide("set_mux_channel",        set_mux_channel);
    Bridge.provide("calibrate_scd30",        calibrate_scd30);
    Bridge.provide("set_matrix_msg",         set_matrix_msg);
    update_scroll_metrics();
}

void loop() {
    scroll_tick();
}

/**
 * Matrix App Sketch
 * Hybrid RobotiX
 *
 * Minimal test — SCD30 + Bridge only, no matrix, no Zephyr thread.
 */

#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>
#include <Wire.h>

Adafruit_SCD30 scd30;

String get_scd_data() {
    if (scd30.dataReady()) {
        scd30.read();
        return String(scd30.CO2) + "," +
               String(scd30.temperature) + "," +
               String(scd30.relative_humidity);
    }
    return "0,0,0";
}

void setup() {
    Bridge.begin();
    while (!scd30.begin(0x61, &Wire1)) { delay(100); }
    Bridge.provide("get_scd_data", get_scd_data);
}

void loop() {
}

#include <Arduino_RouterBridge.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX imager;
VL53L5CX_ResultsData measurementData;

/**
 * Read the 8x8 depth map from the VL53L5CX and return all 64 zone
 * distances as a comma-separated string (mm), ordered row-major
 * from zone 0 (top-left) to zone 63 (bottom-right).
 *
 * Returns "0" if data is not yet ready.
 */
String get_vl53l5cx_data() {
    if (!imager.isDataReady()) {
        return "0";
    }
    if (!imager.getRangingData(&measurementData)) {
        return "0";
    }

    String result = "";
    for (int i = 0; i < 64; i++) {
        result += String(measurementData.distance_mm[i]);
        if (i < 63) {
            result += ",";
        }
    }
    return result;
}

void setup() {
    Bridge.begin();
    Wire1.begin();

    // VL53L5CX firmware upload takes ~10 seconds at power-on —
    // wait until the sensor is ready before proceeding.
    while (!imager.begin(0x29, Wire1)) {
        delay(100);
    }

    // 8x8 resolution, 15 Hz ranging frequency
    imager.setResolution(8 * 8);
    imager.setRangingFrequency(15);
    imager.startRanging();

    Bridge.provide("get_vl53l5cx_data", get_vl53l5cx_data);
}

void loop() {
}

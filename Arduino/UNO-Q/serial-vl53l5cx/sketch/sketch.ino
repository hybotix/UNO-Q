/*
 * VL53L5CX Serial Monitor
 * Hybrid RobotiX - Dale Weber (N7PKT)
 *
 * Reads the 8x8 distance array from the SparkFun Qwiic VL53L5CX ToF
 * sensor and prints each frame to the serial monitor at 115200 baud.
 *
 * Output format: tab-separated distance values (mm), 8 values per row,
 * 8 rows per frame, blank line between frames.
 *
 * Sensor connected to QWIIC bus (Wire1) on the Arduino UNO Q.
 * Sensor firmware upload takes up to 10 seconds at power-on.
 */

#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

SparkFun_VL53L5CX  my_imager;
VL53L5CX_ResultsData measurement_data;

int image_resolution = 0;
int image_width      = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("SparkFun VL53L5CX Imager Example");

    Wire1.begin();

    Serial.println("Initializing sensor board. This can take up to 10s. Please wait.");

    // Sensor initialization failed
    if (my_imager.begin(0x29, Wire1) == false) {
        Serial.println(F("Sensor not found - check your wiring. Freezing"));

        while (1) {
            delay(10);
        }
    }

    my_imager.setResolution(8 * 8);

    image_resolution = my_imager.getResolution();
    image_width      = sqrt(image_resolution);

    my_imager.startRanging();
}

void loop() {
    int x;
    int y;

    // New measurement data available
    if (my_imager.isDataReady() == true) {

        // Ranging data retrieved successfully
        if (my_imager.getRangingData(&measurement_data)) {

            // Iterate over y axis
            for (y = 0; y <= image_width * (image_width - 1); y += image_width) {

                // Iterate over x axis
                for (x = image_width - 1; x >= 0; x--) {
                    Serial.print("\t");
                    Serial.print(measurement_data.distance_mm[x + y]);
                }

                Serial.println();
            }

            Serial.println();
        }
    }

    delay(5);
}

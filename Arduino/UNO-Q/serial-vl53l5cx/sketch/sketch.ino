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

SparkFun_VL53L5CX  myImager;
VL53L5CX_ResultsData measurementData;

int imageResolution = 0;
int imageWidth      = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("SparkFun VL53L5CX Imager Example");

    Wire1.begin();

    Serial.println("Initializing sensor board. This can take up to 10s. Please wait.");

    if (myImager.begin(0x29, Wire1) == false) {
        Serial.println(F("Sensor not found - check your wiring. Freezing"));

        while (1) {
            delay(10);
        }
    }

    myImager.setResolution(8 * 8);

    imageResolution = myImager.getResolution();
    imageWidth      = sqrt(imageResolution);

    myImager.startRanging();
}

void loop() {
    int x;
    int y;

    if (myImager.isDataReady() == true) {
        if (myImager.getRangingData(&measurementData)) {
            for (y = 0; y <= imageWidth * (imageWidth - 1); y += imageWidth) {
                for (x = imageWidth - 1; x >= 0; x--) {
                    Serial.print("\t");
                    Serial.print(measurementData.distance_mm[x + y]);
                }

                Serial.println();
            }

            Serial.println();
        }
    }

    delay(5);
}

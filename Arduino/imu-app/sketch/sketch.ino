#include <Arduino_RouterBridge.h>
#include <Adafruit_LSM6DSOX.h>

Adafruit_LSM6DSOX imu;

String get_lsm6dsox_data() {
    sensors_event_t accel, gyro, temp;
    imu.getEvent(&accel, &gyro, &temp);
    return String(accel.acceleration.x) + "," +
           String(accel.acceleration.y) + "," +
           String(accel.acceleration.z) + "," +
           String(gyro.gyro.x) + "," +
           String(gyro.gyro.y) + "," +
           String(gyro.gyro.z) + "," +
           String(temp.temperature);
}

void setup() {
    Bridge.begin();
    while (!imu.begin_I2C(0x6A, &Wire1)) {
        delay(100);
    }
    Bridge.provide("get_lsm6dsox_data", get_lsm6dsox_data);
}

void loop() {
}

#include <Arduino_RouterBridge.h>
#include <SensirionI2cScd4x.h>

SensirionI2cScd4x scd41;

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

void setup() {
    scd41.begin(Wire1, SCD41_I2C_ADDR_62);
    scd41.startPeriodicMeasurement();

    Bridge.provide("get_scd41_data", get_scd41_data);
}

void loop() {
    delay(10);
}

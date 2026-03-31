#include <Arduino_RouterBridge.h>
#include <Adafruit_SCD30.h>

Adafruit_SCD30 scd30;

String get_scd30_data() {
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
    while (!scd30.begin(0x61, &Wire1)) {
        delay(100);
    }
    Bridge.provide("get_scd30_data", get_scd30_data);
}

void loop() {
}

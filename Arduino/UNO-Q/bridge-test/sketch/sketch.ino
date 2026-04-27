#include <Arduino_RouterBridge.h>

String echo(String value) {
    return value;
}

void setup() {
    Bridge.begin();
    Bridge.provide("echo", echo);
}

void loop() {
}

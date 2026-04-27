#include <Arduino_RouterBridge.h>

int *testAlloc = nullptr;

String echo(String value) {
    return value;
}

void setup() {
    Bridge.begin();
    Bridge.provide("echo", echo);
    testAlloc = new int(42);
}

void loop() {
}

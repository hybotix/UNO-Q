#include <Arduino_RouterBridge.h>

int *test_alloc = nullptr;

String echo(String value) {
    return value;
}

void setup() {
    Bridge.provide("echo", echo);
    Bridge.begin();Bridge.begin();
    test_alloc = new int(42);
}

void loop() {
    delay(10);
}

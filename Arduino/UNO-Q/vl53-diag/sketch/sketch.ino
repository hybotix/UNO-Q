/*
 * VL53L5CX I2C Diagnostic
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests Wire1 communication with VL53L5CX while Bridge is running.
 * Tests: I2C probe, page select write, device ID read, 32-byte write.
 * Does NOT attempt firmware upload.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

static uint8_t wire_write_byte(uint8_t addr, uint16_t reg, uint8_t value) {
    Wire1.beginTransmission(addr);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(value);
    return Wire1.endTransmission();
}

static uint8_t wire_read(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t n) {
    Wire1.beginTransmission(addr);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    uint8_t err = Wire1.endTransmission(false);
    if (err != 0) return err;
    Wire1.requestFrom((uint8_t)addr, n);
    for (uint8_t i = 0; i < n; i++) {
        if (!Wire1.available()) return 4;
        buf[i] = Wire1.read();
    }
    return 0;
}

static uint8_t wire_write_multi(uint8_t addr, uint16_t reg, uint8_t *buf, uint8_t n) {
    Wire1.beginTransmission(addr);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(buf, n);
    return Wire1.endTransmission();
}

void setup() {
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    Wire1.begin();
    delay(500);

    /* Test 1: probe */
    Wire1.beginTransmission(0x29);
    uint8_t err = Wire1.endTransmission();
    if (err != 0) {
        diagResult = "fail:probe:err=" + String(err);
        return;
    }

    /* Test 2: page select + device ID read (expect 0xF0) */
    err = wire_write_byte(0x29, 0x7fff, 0x00);
    if (err != 0) {
        diagResult = "fail:page_select:err=" + String(err);
        return;
    }
    uint8_t device_id = 0;
    err = wire_read(0x29, 0x00, &device_id, 1);
    if (err != 0) {
        diagResult = "fail:read_id:err=" + String(err);
        return;
    }
    if (device_id != 0xF0) {
        diagResult = "fail:bad_id:got=0x" + String(device_id, HEX);
        return;
    }

    /* Test 3: 32-byte write */
    uint8_t testbuf[32];
    for (uint8_t i = 0; i < 32; i++) testbuf[i] = i;
    err = wire_write_multi(0x29, 0x0000, testbuf, 32);
    if (err != 0) {
        diagResult = "fail:write32:err=" + String(err);
        return;
    }

    diagResult = "pass:probe+id(0xF0)+write32";
}

void loop() {}

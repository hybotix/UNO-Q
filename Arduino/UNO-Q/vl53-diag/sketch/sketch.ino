/*
 * VL53L5CX I2C Diagnostic — step 3
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Wire.h included in the SKETCH (not a library) — this is the correct
 * pattern. Including Wire.h in a library auto-inits Wire1 before setup()
 * which hangs the MCU with Bridge running.
 *
 * Tests:
 *   1. I2C probe at 0x29
 *   2. Page select write (reg 0x7FFF = 0x00)
 *   3. Device ID read (reg 0x0000, expect 0xF0)
 *   4. Revision ID read (reg 0x0001, expect 0x02)
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

static uint8_t vl53_write_byte(uint16_t reg, uint8_t value) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(value);
    return Wire1.endTransmission();
}

static uint8_t vl53_read_byte(uint16_t reg, uint8_t &value) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    uint8_t err = Wire1.endTransmission(false);
    if (err != 0) return err;
    Wire1.requestFrom((uint8_t)0x29, (uint8_t)1);
    if (!Wire1.available()) return 4;
    value = Wire1.read();
    return 0;
}

void setup() {
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    /* Test 1: probe */
    Wire1.beginTransmission(0x29);
    uint8_t err = Wire1.endTransmission();
    if (err != 0) {
        diagResult = "fail:probe:err=" + String(err);
        return;
    }

    /* Test 2: page select */
    err = vl53_write_byte(0x7FFF, 0x00);
    if (err != 0) {
        diagResult = "fail:page_select:err=" + String(err);
        return;
    }

    /* Test 3: device ID (expect 0xF0) */
    uint8_t device_id = 0;
    err = vl53_read_byte(0x0000, device_id);
    if (err != 0) {
        diagResult = "fail:read_device_id:err=" + String(err);
        return;
    }
    if (device_id != 0xF0) {
        diagResult = "fail:bad_device_id:got=0x" + String(device_id, HEX);
        return;
    }

    /* Test 4: revision ID (expect 0x02) */
    uint8_t revision_id = 0;
    err = vl53_read_byte(0x0001, revision_id);
    if (err != 0) {
        diagResult = "fail:read_revision_id:err=" + String(err);
        return;
    }
    if (revision_id != 0x02) {
        diagResult = "fail:bad_revision_id:got=0x" + String(revision_id, HEX);
        return;
    }

    diagResult = "pass:probe+page_select+device_id=0xF0+revision_id=0x02";
}

void loop() {}

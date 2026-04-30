/*
 * VL53L5CX I2C Diagnostic — step 7 (find max read size)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * The 4096-byte write passed. Now find the max Wire1.requestFrom() size.
 * Tests reads of 32, 64, 128, 255 bytes from page 0x02.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>

static String diagResult = "not_run";

String get_diag() {
    return diagResult;
}

static uint8_t readBuf[256];

static uint8_t vl53_read_n(uint16_t reg, uint8_t *buf, uint8_t n) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    uint8_t err = Wire1.endTransmission(false);
    if (err != 0) return err;
    uint8_t got = Wire1.requestFrom((uint8_t)0x29, n);
    if (got == 0) return 4;
    for (uint8_t i = 0; i < got; i++) {
        buf[i] = Wire1.available() ? Wire1.read() : 0xFF;
    }
    return (got == n) ? 0 : 5;
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    /* Select page 0x02 */
    Wire1.beginTransmission(0x29);
    Wire1.write(0x7F); Wire1.write(0xFF);
    Wire1.write(0x02);
    if (Wire1.endTransmission() != 0) { diagResult = "fail:page_select"; return; }

    /* Try 32 bytes */
    uint8_t err = vl53_read_n(0x0000, readBuf, 32);
    if (err != 0) { diagResult = "fail:read32:err=" + String(err); return; }

    /* Try 64 bytes */
    err = vl53_read_n(0x0000, readBuf, 64);
    if (err != 0) { diagResult = "fail:read64:err=" + String(err); return; }

    /* Try 128 bytes */
    err = vl53_read_n(0x0000, readBuf, 128);
    if (err != 0) { diagResult = "fail:read128:err=" + String(err); return; }

    /* Try 255 bytes */
    err = vl53_read_n(0x0000, readBuf, 255);
    if (err != 0) { diagResult = "fail:read255:err=" + String(err); return; }

    diagResult = "pass:read32+read64+read128+read255";
}

void loop() {}

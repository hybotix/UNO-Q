/*
 * VL53L5CX I2C Diagnostic — step 6 (large write + read test)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests large Wire1 writes and reads with Bridge running.
 * Simulates the firmware upload chunk size (4096 bytes) and
 * the ranging data read size (~280 bytes).
 *
 * Tests:
 *   1. Probe at 0x29
 *   2. Page select + device ID (sanity check)
 *   3. 4096-byte write to page 0x09 (firmware page — streaming)
 *   4. 256-byte read from page 0x02 (ranging data size)
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

/* Write n bytes to address via Wire1 — simulates WrMulti */
static uint8_t vl53_write_multi(uint16_t reg, uint8_t *buf, size_t n) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(buf, n);
    return Wire1.endTransmission();
}

/* Read n bytes from address via Wire1 — simulates RdMulti */
static uint8_t vl53_read_multi(uint16_t reg, uint8_t *buf, size_t n) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    uint8_t err = Wire1.endTransmission(false);
    if (err != 0) return err;
    Wire1.requestFrom((uint8_t)0x29, (uint8_t)n);
    for (size_t i = 0; i < n; i++) {
        if (!Wire1.available()) return 4;
        buf[i] = Wire1.read();
    }
    return 0;
}

static uint8_t writeBuf[4096];
static uint8_t readBuf[256];

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    /* Test 1: probe */
    Wire1.beginTransmission(0x29);
    uint8_t err = Wire1.endTransmission();
    if (err != 0) { diagResult = "fail:probe:err=" + String(err); return; }

    /* Test 2: device ID sanity */
    err = vl53_write_byte(0x7FFF, 0x00);
    if (err != 0) { diagResult = "fail:page_select:err=" + String(err); return; }
    uint8_t device_id = 0;
    err = vl53_read_byte(0x0000, device_id);
    if (err != 0 || device_id != 0xF0) {
        diagResult = "fail:device_id:err=" + String(err) + ":got=0x" + String(device_id, HEX);
        return;
    }

    /* Test 3: 4096-byte write to firmware page 0x09 */
    err = vl53_write_byte(0x7FFF, 0x09);
    if (err != 0) { diagResult = "fail:select_page09:err=" + String(err); return; }
    for (int i = 0; i < 4096; i++) writeBuf[i] = (uint8_t)(i & 0xFF);
    err = vl53_write_multi(0x0000, writeBuf, 4096);
    if (err != 0) { diagResult = "fail:write4096:err=" + String(err); return; }

    /* Test 4: 256-byte read from page 0x02 */
    err = vl53_write_byte(0x7FFF, 0x02);
    if (err != 0) { diagResult = "fail:select_page02:err=" + String(err); return; }
    err = vl53_read_multi(0x0000, readBuf, 256);
    if (err != 0) { diagResult = "fail:read256:err=" + String(err); return; }

    diagResult = "pass:probe+id+write4096+read256";
}

void loop() {}

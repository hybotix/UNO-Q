/*
 * VL53L5CX I2C Diagnostic — step 9 (firmware page 1 only)
 * Hybrid RobotiX — Dale Weber <hybotix@hybridrobotix.io>
 *
 * Tests writing just the first firmware page (0x8000 = 32768 bytes)
 * in 4096-byte chunks to page 0x09. Reports progress after each chunk.
 */

#include <Arduino_RouterBridge.h>
#include <Wire.h>
#include <hybx_vl53l5cx.h>

/* Include the firmware buffer directly */
#include "/home/arduino/Arduino/libraries/hybx_vl53l5cx/src/uld/vl53l5cx_buffers.h"

static String diagResult = "not_run";
static uint8_t chunksDone = 0;

String get_diag() {
    return diagResult;
}

static uint8_t wire_write_byte(uint16_t reg, uint8_t value) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(value);
    return Wire1.endTransmission();
}

static uint8_t wire_write_chunk(uint16_t reg, const uint8_t *buf, size_t n) {
    Wire1.beginTransmission(0x29);
    Wire1.write((uint8_t)(reg >> 8));
    Wire1.write((uint8_t)(reg & 0xFF));
    Wire1.write(buf, n);
    return Wire1.endTransmission();
}

void setup() {
    Wire1.begin();
    Bridge.begin();
    Bridge.provide("get_diag", get_diag);

    /* Select firmware page 0x09 */
    uint8_t err = wire_write_byte(0x7fff, 0x09);
    if (err != 0) { diagResult = "fail:page09:err=" + String(err); return; }

    /* Write 0x8000 bytes in 4096-byte chunks */
    for (uint32_t offset = 0; offset < 0x8000; offset += 4096) {
        uint32_t chunk = min((uint32_t)4096, (uint32_t)0x8000 - offset);
        err = wire_write_chunk(0x0000, &VL53L5CX_FIRMWARE[offset], chunk);
        if (err != 0) {
            diagResult = "fail:write_chunk:offset=" + String(offset) +
                         ":err=" + String(err);
            return;
        }
        chunksDone++;
        diagResult = "writing:chunk=" + String(chunksDone) +
                     "/8:offset=" + String(offset);
        delay(10);
    }

    diagResult = "pass:page09_written_8x4096";
}

void loop() {}

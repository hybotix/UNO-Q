/*
 * platform.cpp
 * Hybrid RobotiX — hybx_vl53l5cx
 * Dale Weber (N7PKT)
 *
 * ST ULD platform adaptation — Wire I2C implementation.
 * No SparkFun dependency whatsoever.
 *
 * I2C notes
 * ---------
 * The VL53L5CX uses 16-bit register addresses sent MSB-first.
 *
 * Zephyr's Wire implementation has an internal buffer of 256 bytes.
 * WrMulti chunks large writes (firmware upload) into HYBX_I2C_WR_CHUNK
 * byte pages, re-issuing the 16-bit register address at the start of
 * each chunk. The sensor auto-increments internally so we advance the
 * register address by chunk size on each iteration.
 *
 * RdMulti reads in 32-byte chunks (Wire requestFrom limit on Zephyr).
 *
 * License: MIT (our code) — ST ULD files carry BSD 3-clause.
 */

#include "platform.h"
#include <Arduino.h>

/* Maximum bytes per WrMulti chunk — must leave room for 2-byte address */
#define HYBX_I2C_WR_CHUNK  250U
/* Maximum bytes per RdMulti request */
#define HYBX_I2C_RD_CHUNK   32U

/* -------------------------------------------------------------------------
 * RdByte
 * -------------------------------------------------------------------------*/
extern "C" uint8_t RdByte(VL53L5CX_Platform *p_platform,
                           uint16_t RegisterAddress, uint8_t *p_value)
{
    TwoWire *wire = p_platform->wire;
    uint8_t  addr = (uint8_t)(p_platform->address);

    wire->beginTransmission(addr);
    wire->write((uint8_t)(RegisterAddress >> 8));
    wire->write((uint8_t)(RegisterAddress & 0xFF));
    if (wire->endTransmission(false) != 0) {
        return 1;
    }
    if (wire->requestFrom(addr, (uint8_t)1) != 1) {
        return 1;
    }
    *p_value = wire->read();
    return 0;
}

/* -------------------------------------------------------------------------
 * WrByte
 * -------------------------------------------------------------------------*/
extern "C" uint8_t WrByte(VL53L5CX_Platform *p_platform,
                           uint16_t RegisterAddress, uint8_t value)
{
    TwoWire *wire = p_platform->wire;
    uint8_t  addr = (uint8_t)(p_platform->address);

    wire->beginTransmission(addr);
    wire->write((uint8_t)(RegisterAddress >> 8));
    wire->write((uint8_t)(RegisterAddress & 0xFF));
    wire->write(value);
    return (wire->endTransmission() == 0) ? 0 : 1;
}

/* -------------------------------------------------------------------------
 * RdMulti — read size bytes from RegisterAddress into p_values.
 * Chunked at HYBX_I2C_RD_CHUNK bytes per requestFrom call.
 * -------------------------------------------------------------------------*/
extern "C" uint8_t RdMulti(VL53L5CX_Platform *p_platform,
                            uint16_t RegisterAddress,
                            uint8_t *p_values, uint32_t size)
{
    TwoWire *wire = p_platform->wire;
    uint8_t  addr = (uint8_t)(p_platform->address);

    /* Set register pointer */
    wire->beginTransmission(addr);
    wire->write((uint8_t)(RegisterAddress >> 8));
    wire->write((uint8_t)(RegisterAddress & 0xFF));
    if (wire->endTransmission(false) != 0) {
        return 1;
    }

    uint32_t remaining = size;
    uint32_t offset    = 0;

    while (remaining > 0) {
        uint8_t chunk = (remaining > HYBX_I2C_RD_CHUNK)
                        ? (uint8_t)HYBX_I2C_RD_CHUNK
                        : (uint8_t)remaining;
        uint8_t got = wire->requestFrom(addr, chunk);
        if (got != chunk) {
            return 1;
        }
        for (uint8_t i = 0; i < got; i++) {
            p_values[offset++] = wire->read();
        }
        remaining -= got;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * WrMulti — write size bytes from p_values to RegisterAddress.
 *
 * Chunked at HYBX_I2C_WR_CHUNK bytes. Each chunk re-issues the register
 * address (incremented by the number of bytes already written) because the
 * sensor's internal pointer auto-increments and we must stay in sync across
 * Wire transaction boundaries.
 * -------------------------------------------------------------------------*/
extern "C" uint8_t WrMulti(VL53L5CX_Platform *p_platform,
                            uint16_t RegisterAddress,
                            uint8_t *p_values, uint32_t size)
{
    TwoWire *wire = p_platform->wire;
    uint8_t  addr = (uint8_t)(p_platform->address);

    uint32_t remaining = size;
    uint32_t offset    = 0;
    uint16_t reg       = RegisterAddress;

    while (remaining > 0) {
        uint32_t chunk = (remaining > HYBX_I2C_WR_CHUNK)
                         ? HYBX_I2C_WR_CHUNK
                         : remaining;

        wire->beginTransmission(addr);
        wire->write((uint8_t)(reg >> 8));
        wire->write((uint8_t)(reg & 0xFF));
        for (uint32_t i = 0; i < chunk; i++) {
            wire->write(p_values[offset + i]);
        }
        if (wire->endTransmission() != 0) {
            return 1;
        }

        offset    += chunk;
        reg       += (uint16_t)chunk;
        remaining -= chunk;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * SwapBuffer — big-endian <-> little-endian word swap in place.
 * Size is always a multiple of 4.
 * -------------------------------------------------------------------------*/
extern "C" void SwapBuffer(uint8_t *buffer, uint16_t size)
{
    uint32_t tmp;
    for (uint16_t i = 0; i < size; i += 4) {
        tmp = ((uint32_t)buffer[i    ] << 24)
            | ((uint32_t)buffer[i + 1] << 16)
            | ((uint32_t)buffer[i + 2] <<  8)
            | ((uint32_t)buffer[i + 3]);
        memcpy(&buffer[i], &tmp, 4);
    }
}

/* -------------------------------------------------------------------------
 * WaitMs — blocking delay.
 * -------------------------------------------------------------------------*/
extern "C" uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs)
{
    (void)p_platform;
    delay(TimeMs);
    return 0;
}

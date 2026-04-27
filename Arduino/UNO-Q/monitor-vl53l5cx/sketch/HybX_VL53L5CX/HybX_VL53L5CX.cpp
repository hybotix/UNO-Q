/*
 * HybX_VL53L5CX.cpp
 * Hybrid RobotiX — Dale Weber (N7PKT)
 *
 * Implementation of the minimal, heap-free VL53L5CX driver.
 * See HybX_VL53L5CX.h for full design notes.
 *
 * PLATFORM LAYER
 * --------------
 * The ST ULD requires a platform adaptation layer: VL53L5CX_RdByte,
 * VL53L5CX_WrByte, VL53L5CX_RdMulti, VL53L5CX_WrMulti, VL53L5CX_WaitMs,
 * VL53L5CX_Reset_Sensor, and VL53L5CX_SwapBuffer.
 *
 * We implement these directly here using Wire (Arduino TwoWire).  The I2C
 * address and TwoWire pointer are stored in VL53L5CX_Platform, which is a
 * member of VL53L5CX_Configuration (_dev).
 *
 * The ULD sends large multi-byte writes (up to 1452 bytes for firmware
 * upload).  Wire.h on Zephyr has a 256-byte I2C buffer limit.  We chunk
 * WrMulti into 250-byte pages, preserving the 16-bit register address in
 * every chunk header.
 */

#include "HybX_VL53L5CX.h"

/* -------------------------------------------------------------------------
 * Static definitions
 * -------------------------------------------------------------------------*/

/* The driver configuration (contains temp_buffer[1452]) — BSS, not heap. */
VL53L5CX_Configuration HybX_VL53L5CX::_dev;

/* Public result arrays — BSS, not heap. */
int16_t hybx_distance_mm[64];
uint8_t hybx_target_status[64];
bool    hybx_sensor_ready = false;

/* -------------------------------------------------------------------------
 * Constructor
 * -------------------------------------------------------------------------*/
HybX_VL53L5CX::HybX_VL53L5CX(uint8_t resolution,
                               uint8_t address,
                               TwoWire &wire)
    : _resolution(resolution), _initialized(false)
{
    _dev.platform.address = address;
    _dev.platform.wire    = &wire;
}

/* -------------------------------------------------------------------------
 * begin() — firmware upload + start ranging
 * -------------------------------------------------------------------------*/
bool HybX_VL53L5CX::begin() {
    uint8_t status;

    /* Init the ULD — uploads firmware over I2C, takes up to 10 s. */
    status = vl53l5cx_init(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    /* Enable only the two result types we need. */
    status = vl53l5cx_set_resolution(&_dev, _resolution);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    /* Set ranging frequency: 15 Hz for 8×8, 30 Hz for 4×4. */
    uint8_t freq = (_resolution == 64) ? 15 : 30;
    status = vl53l5cx_set_ranging_frequency_hz(&_dev, freq);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    status = vl53l5cx_start_ranging(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    _initialized = true;
    return true;
}

/* -------------------------------------------------------------------------
 * setResolution() — reconfigure resolution at runtime
 * -------------------------------------------------------------------------*/
bool HybX_VL53L5CX::setResolution(uint8_t resolution) {
    if (!_initialized) {
        return false;
    }
    if (resolution != 16 && resolution != 64) {
        return false;
    }

    uint8_t status;

    status = vl53l5cx_stop_ranging(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    _resolution = resolution;

    status = vl53l5cx_set_resolution(&_dev, _resolution);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    uint8_t freq = (_resolution == 64) ? 15 : 30;
    vl53l5cx_set_ranging_frequency_hz(&_dev, freq);

    status = vl53l5cx_start_ranging(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------
 * poll() — non-blocking, call from loop()
 * -------------------------------------------------------------------------*/
void HybX_VL53L5CX::poll() {
    if (!_initialized) {
        return;
    }

    uint8_t isReady = 0;
    vl53l5cx_check_data_ready(&_dev, &isReady);
    if (isReady) {
        _readFrame();
    }
}

/* -------------------------------------------------------------------------
 * _readFrame() — reads one frame into static globals
 *
 * We declare a minimal local ResultsData struct.  However, the ST ULD's
 * vl53l5cx_get_ranging_data() writes into VL53L5CX_ResultsData which is
 * defined by the ULD header and may be large.  To avoid stack overflow we
 * declare it static here — it is only ever accessed from this function,
 * which is not reentrant, so static is safe.
 * -------------------------------------------------------------------------*/
void HybX_VL53L5CX::_readFrame() {
    /* Static: keeps the large struct off the stack. */
    static VL53L5CX_ResultsData results;

    uint8_t status = vl53l5cx_get_ranging_data(&_dev, &results);
    if (status != VL53L5CX_STATUS_OK) {
        return;
    }

    /* Copy only the two arrays we care about. */
    uint8_t zones = _resolution;  /* 16 or 64 */
    for (uint8_t i = 0; i < zones; i++) {
        hybx_distance_mm[i]  = results.distance_mm[i];
        hybx_target_status[i] = results.target_status[i];
    }

    hybx_sensor_ready = true;
}

/* ==========================================================================
 * ST ULD Platform Adaptation Layer
 * ==========================================================================
 *
 * The ULD calls these C functions.  They must have C linkage.
 *
 * I2C register addresses on the VL53L5CX are 16-bit, sent MSB-first.
 * Wire on Zephyr has a ~256-byte internal buffer; we chunk large writes
 * into 250-byte pages.
 */

extern "C" {

/* Zephyr Wire buffer is 256 bytes; leave 2 for the 16-bit register address
 * and a few bytes of margin. */
#define HYBX_I2C_CHUNK 250

static inline TwoWire *_wire(VL53L5CX_Platform *p) {
    return p->wire;
}
static inline uint8_t _addr(VL53L5CX_Platform *p) {
    return (uint8_t)(p->address);
}

/* ---------- RdByte ------------------------------------------------------- */
uint8_t VL53L5CX_RdByte(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t *p_value)
{
    TwoWire *wire = _wire(p_platform);
    uint8_t  addr = _addr(p_platform);

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

/* ---------- WrByte ------------------------------------------------------- */
uint8_t VL53L5CX_WrByte(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t value)
{
    TwoWire *wire = _wire(p_platform);
    uint8_t  addr = _addr(p_platform);

    wire->beginTransmission(addr);
    wire->write((uint8_t)(RegisterAddress >> 8));
    wire->write((uint8_t)(RegisterAddress & 0xFF));
    wire->write(value);
    return (wire->endTransmission() == 0) ? 0 : 1;
}

/* ---------- RdMulti ------------------------------------------------------ */
uint8_t VL53L5CX_RdMulti(VL53L5CX_Platform *p_platform,
                          uint16_t RegisterAddress,
                          uint8_t *p_values, uint32_t size)
{
    TwoWire *wire = _wire(p_platform);
    uint8_t  addr = _addr(p_platform);

    wire->beginTransmission(addr);
    wire->write((uint8_t)(RegisterAddress >> 8));
    wire->write((uint8_t)(RegisterAddress & 0xFF));
    if (wire->endTransmission(false) != 0) {
        return 1;
    }

    uint32_t remaining = size;
    uint32_t offset    = 0;

    while (remaining > 0) {
        uint8_t  chunk = (remaining > 32) ? 32 : (uint8_t)remaining;
        uint8_t  got   = wire->requestFrom(addr, chunk);
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

/* ---------- WrMulti ------------------------------------------------------ */
/*
 * Large writes (firmware upload = 84 KB in total, split across many calls by
 * the ULD) arrive here in pieces.  Each call is at most a few hundred bytes;
 * we re-chunk at HYBX_I2C_CHUNK boundaries, re-issuing the register address
 * at the start of each chunk because the sensor auto-increments internally.
 */
uint8_t VL53L5CX_WrMulti(VL53L5CX_Platform *p_platform,
                          uint16_t RegisterAddress,
                          uint8_t *p_values, uint32_t size)
{
    TwoWire *wire = _wire(p_platform);
    uint8_t  addr = _addr(p_platform);

    uint32_t remaining = size;
    uint32_t offset    = 0;
    uint16_t reg       = RegisterAddress;

    while (remaining > 0) {
        uint32_t chunk = (remaining > HYBX_I2C_CHUNK) ? HYBX_I2C_CHUNK : remaining;

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

/* ---------- Reset -------------------------------------------------------- */
/*
 * The VL53L5CX breakout has no dedicated RESET pin accessible from software
 * on this wiring.  The ULD calls this once during init; we perform a soft
 * reset by writing to the sensor's SOFT_RESET register instead.
 */
uint8_t VL53L5CX_Reset_Sensor(VL53L5CX_Platform *p_platform) {
    /* SOFT_RESET register = 0x7FFF, assert reset (value 0x00), then release (0x01). */
    VL53L5CX_WrByte(p_platform, 0x7FFF, 0x00);
    delay(10);
    VL53L5CX_WrByte(p_platform, 0x7FFF, 0x01);
    delay(10);
    return 0;
}

/* ---------- SwapBuffer --------------------------------------------------- */
/*
 * Byte-swap a buffer in-place (little-endian ↔ big-endian for uint32_t
 * words).  Used by the ULD when parsing firmware data.
 */
void VL53L5CX_SwapBuffer(uint8_t *buffer, uint16_t size) {
    uint32_t i, tmp;
    for (i = 0; i < (uint32_t)size; i += 4) {
        tmp           = (uint32_t)buffer[i + 0] << 24
                      | (uint32_t)buffer[i + 1] << 16
                      | (uint32_t)buffer[i + 2] << 8
                      | (uint32_t)buffer[i + 3];
        buffer[i + 0] = (tmp >> 24) & 0xFF;
        buffer[i + 1] = (tmp >> 16) & 0xFF;
        buffer[i + 2] = (tmp >>  8) & 0xFF;
        buffer[i + 3] =  tmp        & 0xFF;
    }
}

/* ---------- WaitMs ------------------------------------------------------- */
uint8_t VL53L5CX_WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs) {
    (void)p_platform;
    delay(TimeMs);
    return 0;
}

}  /* extern "C" */

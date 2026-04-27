/*
 * hybx_vl53l5cx.cpp
 * Hybrid RobotiX — hybx_vl53l5cx
 * Dale Weber (N7PKT)
 *
 * See hybx_vl53l5cx.h for full design notes.
 *
 * License: MIT (our code) — ST ULD files carry BSD 3-clause.
 */

#include "hybx_vl53l5cx.h"

/* -------------------------------------------------------------------------
 * Static definitions — all in BSS, zero heap.
 * -------------------------------------------------------------------------*/

/* ST ULD configuration struct (~5375 bytes including temp_buffer). */
VL53L5CX_Configuration hybx_vl53l5cx::_dev;

/* Public result globals. */
int16_t hybx_distance_mm[64];
uint8_t hybx_target_status[64];
bool    hybx_sensor_ready = false;

/* -------------------------------------------------------------------------
 * Constructor
 * -------------------------------------------------------------------------*/
hybx_vl53l5cx::hybx_vl53l5cx(uint8_t resolution,
                               uint8_t address,
                               TwoWire &wire)
    : _resolution(resolution), _initialized(false)
{
    _dev.platform.address = (uint16_t)address;
    _dev.platform.wire    = &wire;
}

/* -------------------------------------------------------------------------
 * begin() — firmware upload and start ranging.
 * -------------------------------------------------------------------------*/
bool hybx_vl53l5cx::begin()
{
    uint8_t status;

    /* Upload sensor firmware — this is the slow step (~10 s over I2C). */
    status = vl53l5cx_init(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    status = vl53l5cx_set_resolution(&_dev, _resolution);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    /* 8x8 max 15 Hz, 4x4 max 60 Hz — use 15 and 30 as sensible defaults. */
    uint8_t freq = (_resolution == VL53L5CX_RESOLUTION_8X8) ? 15 : 30;
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
 * setResolution() — stop, reconfigure, restart.
 * -------------------------------------------------------------------------*/
bool hybx_vl53l5cx::setResolution(uint8_t resolution)
{
    if (!_initialized) {
        return false;
    }
    if (resolution != VL53L5CX_RESOLUTION_4X4 &&
        resolution != VL53L5CX_RESOLUTION_8X8) {
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

    uint8_t freq = (_resolution == VL53L5CX_RESOLUTION_8X8) ? 15 : 30;
    vl53l5cx_set_ranging_frequency_hz(&_dev, freq);

    status = vl53l5cx_start_ranging(&_dev);
    if (status != VL53L5CX_STATUS_OK) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------
 * poll() — non-blocking, call from loop().
 * -------------------------------------------------------------------------*/
void hybx_vl53l5cx::poll()
{
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
 * _readFrame() — read one frame into static result globals.
 *
 * VL53L5CX_ResultsData is declared static here to keep it out of both
 * the heap and the stack. _readFrame() is only ever called from poll()
 * which is not reentrant, so static is safe.
 * -------------------------------------------------------------------------*/
void hybx_vl53l5cx::_readFrame()
{
    static VL53L5CX_ResultsData results;

    uint8_t status = vl53l5cx_get_ranging_data(&_dev, &results);
    if (status != VL53L5CX_STATUS_OK) {
        return;
    }

    uint8_t zones = _resolution;   /* 16 or 64 */
    for (uint8_t i = 0; i < zones; i++) {
        hybx_distance_mm[i]  = results.distance_mm[i];
        hybx_target_status[i] = results.target_status[i];
    }

    hybx_sensor_ready = true;
}

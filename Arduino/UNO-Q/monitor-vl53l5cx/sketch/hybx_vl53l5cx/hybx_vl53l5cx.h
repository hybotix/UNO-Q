/*
 * hybx_vl53l5cx.h
 * Hybrid RobotiX — hybx_vl53l5cx
 * Dale Weber (N7PKT)
 *
 * Minimal, heap-free VL53L5CX driver for the Arduino UNO Q / Zephyr RTOS.
 *
 * DESIGN CONSTRAINTS
 * ------------------
 * The Arduino RouterBridge on Zephyr RTOS is incompatible with operator new:
 * any heap allocation that occurs at or after Bridge.begin() / Bridge.provide()
 * corrupts the Bridge's internal String parameter registration table.
 *
 * This library uses ZERO heap. Every buffer — including the ST ULD's
 * temp_buffer inside VL53L5CX_Configuration — is a static global placed
 * in BSS by the linker at link time.
 *
 * Only two result arrays are allocated:
 *   hybx_distance_mm[64]     int16_t  millimetres, zones 0–63
 *   hybx_target_status[64]   uint8_t  ST status codes (5 or 9 = valid)
 *
 * All other VL53L5CX result fields are disabled via DISABLE macros in
 * platform.h, shrinking temp_buffer and the I2C payload to the minimum.
 *
 * TEMP BUFFER SIZE
 * ----------------
 * With only distance_mm and target_status enabled at NB_TARGET_PER_ZONE=1:
 *   L5CX_DIST_SIZE   = (128 * 1) + 4 = 132
 *   L5CX_STA_SIZE    = (64  * 1) + 4 = 68
 *   All others       = 0
 *   VL53L5CX_MAX_RESULTS_SIZE = 40 + 132 + 68 + 8 = 248
 *   VL53L5CX_TEMPORARY_BUFFER_SIZE = max(248, 1024) = 4096 (ULD minimum)
 *
 * VL53L5CX_Configuration total (approx):
 *   platform (6B) + scalars (9B) + offset_data (488B) +
 *   xtalk_data (776B) + temp_buffer (4096B) = ~5375 bytes — all in BSS.
 *
 * USAGE
 * -----
 *   #include <hybx_vl53l5cx.h>
 *
 *   hybx_vl53l5cx sensor;
 *
 *   void setup() {
 *       Bridge.begin();
 *       Bridge.provide("get_distance_data", get_distance_data);
 *       Bridge.provide("get_target_status", get_target_status);
 *       Bridge.provide("set_resolution",    set_resolution);
 *       Wire1.begin();
 *       sensor.begin();    // uploads firmware — up to 10 s
 *   }
 *
 *   void loop() {
 *       sensor.poll();     // non-blocking; populates result globals
 *   }
 *
 * RESULT GLOBALS (read from Bridge functions)
 * -------------------------------------------
 *   hybx_distance_mm[64]    — latest distance per zone in mm
 *   hybx_target_status[64]  — latest status per zone (5 or 9 = valid)
 *   hybx_sensor_ready       — true once the first frame has arrived
 *
 * WIRE
 * ----
 * Default: Wire1 (UNO Q QWIIC bus), address 0x29.
 * Wire1.begin() must be called before sensor.begin().
 *
 * License: MIT (our code) — ST ULD files carry BSD 3-clause.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "platform.h"
#include "uld/vl53l5cx_api.h"

/* -------------------------------------------------------------------------
 * Public result globals — defined in hybx_vl53l5cx.cpp, BSS allocation.
 * Bridge functions read directly from these arrays.
 * -------------------------------------------------------------------------*/
extern int16_t hybx_distance_mm[64];
extern uint8_t hybx_target_status[64];
extern bool    hybx_sensor_ready;

/* -------------------------------------------------------------------------
 * hybx_vl53l5cx — thin driver class.
 *
 * VL53L5CX_Configuration is a static member: the linker places the entire
 * struct (including the 4096-byte temp_buffer) in BSS — zero heap use.
 * -------------------------------------------------------------------------*/
class hybx_vl53l5cx {
public:
    /*
     * Constructor.
     * resolution : 16 (4x4) or 64 (8x8). Default 64.
     * address    : 7-bit I2C address. Default 0x29.
     * wire       : Wire instance. Default Wire1 (UNO Q QWIIC).
     */
    hybx_vl53l5cx(uint8_t resolution = 64,
                  uint8_t address    = 0x29,
                  TwoWire &wire      = Wire1);

    /*
     * begin() — upload firmware and start ranging.
     * Call from setup(), AFTER Bridge.begin() and all Bridge.provide() calls.
     * Wire1.begin() must already have been called.
     * Blocks up to ~10 s while firmware uploads over I2C.
     * Returns true on success.
     */
    bool begin();

    /*
     * poll() — call from loop().
     * Non-blocking. Checks the sensor for new data; when ready, reads one
     * frame and updates hybx_distance_mm[] and hybx_target_status[].
     * Sets hybx_sensor_ready = true after the first successful frame.
     */
    void poll();

    /*
     * setResolution() — change resolution at runtime.
     * resolution: 16 (4x4) or 64 (8x8).
     * Stops ranging, reconfigures, restarts ranging.
     * Returns true on success.
     */
    bool setResolution(uint8_t resolution);

    /* Returns the currently active resolution (16 or 64). */
    uint8_t getResolution() const { return _resolution; }

private:
    /*
     * _dev — the ST ULD configuration struct.
     * Static: lives in BSS, never touches the heap.
     * Contains temp_buffer[4096], offset_data[488], xtalk_data[776].
     */
    static VL53L5CX_Configuration _dev;

    uint8_t _resolution;
    bool    _initialized;

    /*
     * _readFrame() — read one ranging frame from the sensor into the
     * public result globals. Called by poll() when data is ready.
     * Internal VL53L5CX_ResultsData is also static (avoids stack overflow).
     */
    void _readFrame();
};

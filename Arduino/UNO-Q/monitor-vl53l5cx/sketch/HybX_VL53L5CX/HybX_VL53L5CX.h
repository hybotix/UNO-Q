/*
 * HybX_VL53L5CX.h
 * Hybrid RobotiX — Dale Weber (N7PKT)
 *
 * Minimal VL53L5CX driver for the Arduino UNO Q / Zephyr RTOS.
 *
 * DESIGN CONSTRAINTS
 * ------------------
 * The Arduino RouterBridge on Zephyr RTOS is incompatible with operator new:
 * any heap allocation that occurs at or after Bridge.begin() / Bridge.provide()
 * corrupts the Bridge's internal String parameter registration table.
 *
 * This library therefore uses ZERO heap.  Every buffer — including the ST ULD's
 * 1452-byte temp_buffer that lives inside VL53L5CX_Configuration — is declared
 * as a static global and placed in BSS by the linker.
 *
 * Only two result arrays are allocated:
 *   distance_mm[64]     — int16_t, millimetres, zones 0-63
 *   target_status[64]   — uint8_t, ST status codes (5 or 9 = valid)
 *
 * All other VL53L5CX_ResultsData fields (reflectance, signal, sigma, …) are
 * silently discarded by requesting only the two enabled results via
 * VL53L5CX_NB_TARGET_PER_ZONE = 1 and a custom results mask.
 *
 * USAGE
 * -----
 *   #include "HybX_VL53L5CX/HybX_VL53L5CX.h"
 *
 *   HybX_VL53L5CX vl53;
 *
 *   void setup() {
 *       Bridge.begin();
 *       Bridge.provide("set_resolution",    vl53_set_resolution);
 *       Bridge.provide("get_distance_data", get_distance_data);
 *       Bridge.provide("get_target_status", get_target_status);
 *   }
 *
 *   void loop() {
 *       vl53.poll();   // non-blocking; sets sensorReady when init completes
 *   }
 *
 * WIRE
 * ----
 * Sensor must be on Wire1 (UNO Q QWIIC bus), address 0x29.
 * Wire1.begin() must be called before HybX_VL53L5CX::begin().
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

/* --------------------------------------------------------------------------
 * ST ULD platform types — the ULD C API needs these typedefs before we can
 * include vl53l5cx_api.h.  They are normally provided by platform.h inside
 * the SparkFun library.  We redeclare just what we need so we can call the
 * ULD directly without pulling in the SparkFun wrapper (which instantiates
 * the heavy VL53L5CX_ResultsData struct on the stack).
 * -------------------------------------------------------------------------- */
#ifndef VL53L5CX_PLATFORM_H
#define VL53L5CX_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

/* The ULD platform layer uses this opaque handle for I2C comms. */
typedef struct {
    uint16_t  address;       /* 7-bit I2C address (default 0x29) */
    TwoWire  *wire;          /* pointer to the Wire instance to use */
} VL53L5CX_Platform;

/* ULD platform API — implementations in HybX_VL53L5CX.cpp */
uint8_t VL53L5CX_RdByte(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t *p_value);
uint8_t VL53L5CX_WrByte(VL53L5CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t value);
uint8_t VL53L5CX_RdMulti(VL53L5CX_Platform *p_platform,
                          uint16_t RegisterAddress,
                          uint8_t *p_values, uint32_t size);
uint8_t VL53L5CX_WrMulti(VL53L5CX_Platform *p_platform,
                          uint16_t RegisterAddress,
                          uint8_t *p_values, uint32_t size);
uint8_t VL53L5CX_Reset_Sensor(VL53L5CX_Platform *p_platform);
void    VL53L5CX_SwapBuffer(uint8_t *buffer, uint16_t size);
uint8_t VL53L5CX_WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs);

#endif  /* VL53L5CX_PLATFORM_H */

/* -------------------------------------------------------------------------
 * Pull in the ST ULD API.  The SparkFun library ships the ULD headers at
 * src/vl53l5cx_api.h relative to the Arduino libraries folder.  Because we
 * are inside a sketch subfolder, the Arduino build system adds the sketch
 * folder AND the sketch/HybX_VL53L5CX folder to the include path, so we
 * use an angle-bracket include that the board package resolves via its
 * library search path (SparkFun VL53L5CX Arduino Library must still appear
 * in sketch.yaml — we just stop using the SparkFun C++ wrapper class).
 * -------------------------------------------------------------------------*/
#include <vl53l5cx_api.h>

/* -------------------------------------------------------------------------
 * Result mask: enable only distance_mm and target_status to minimise the
 * data transferred over I2C and avoid populating unused fields.
 * -------------------------------------------------------------------------*/
#ifndef VL53L5CX_RESULT_DISTANCE_MM
/* Bit positions from UM2884 — ST ULD reference manual §3.5 */
#define VL53L5CX_RESULT_DISTANCE_MM      (1U << 1)
#define VL53L5CX_RESULT_TARGET_STATUS    (1U << 8)
#endif

#define HYBX_RESULT_MASK \
    (VL53L5CX_RESULT_DISTANCE_MM | VL53L5CX_RESULT_TARGET_STATUS)

/* -------------------------------------------------------------------------
 * Static result storage — the ONLY heap-free result arrays we need.
 * Declared extern here; defined once in HybX_VL53L5CX.cpp.
 * -------------------------------------------------------------------------*/
extern int16_t  hybx_distance_mm[64];
extern uint8_t  hybx_target_status[64];
extern bool     hybx_sensor_ready;

/* -------------------------------------------------------------------------
 * HybX_VL53L5CX — thin driver class
 *
 * Internals: VL53L5CX_Configuration is a static member (BSS) so the 1452-
 * byte temp_buffer it contains never touches the heap.
 * -------------------------------------------------------------------------*/
class HybX_VL53L5CX {
public:
    /* resolution: 16 (4×4) or 64 (8×8).  Default 64. */
    HybX_VL53L5CX(uint8_t resolution = 64,
                  uint8_t address    = 0x29,
                  TwoWire &wire      = Wire1);

    /*
     * begin() — call once from setup(), AFTER Bridge.begin() and all
     * Bridge.provide() calls.  Uploads sensor firmware (≤10 s) and starts
     * ranging.  Returns true on success.
     */
    bool begin();

    /*
     * poll() — call from loop().  Non-blocking.  On first call it drives
     * the deferred initialization state machine; thereafter it reads frames
     * whenever the sensor signals data ready.
     *
     * Results land in hybx_distance_mm[] and hybx_target_status[].
     * hybx_sensor_ready becomes true after the first successful frame.
     */
    void poll();

    /*
     * setResolution() — 16 for 4×4, 64 for 8×8.
     * Stops ranging, reconfigures, restarts ranging.
     * Safe to call any time after begin().
     */
    bool setResolution(uint8_t resolution);

    uint8_t getResolution() const { return _resolution; }

private:
    /* Static so the struct lives in BSS, not the heap. */
    static VL53L5CX_Configuration _dev;

    uint8_t  _resolution;
    bool     _initialized;

    void _readFrame();
};

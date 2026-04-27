/*
 * platform.h
 * Hybrid RobotiX — hybx_vl53l5cx
 * Dale Weber (N7PKT)
 *
 * ST ULD platform adaptation layer for the Arduino UNO Q / Zephyr RTOS.
 *
 * This file satisfies the ST ULD's #include "platform.h" requirement.
 * It defines VL53L5CX_Platform and declares the six mandatory platform
 * functions that vl53l5cx_api.cpp calls. All implementations are in
 * platform.cpp and use Arduino Wire directly — no SparkFun dependency.
 *
 * Configuration macros
 * --------------------
 * VL53L5CX_NB_TARGET_PER_ZONE   — targets reported per zone (1–4). We
 *                                  use 1: minimises I2C payload and keeps
 *                                  result arrays at exactly [64] elements.
 *
 * All other output fields (ambient_per_spad, nb_spads_enabled, etc.) are
 * disabled via the DISABLE macros below, shrinking temp_buffer and the
 * over-the-wire I2C payload to the minimum needed for distance + status.
 *
 * License: MIT (our code) — ST ULD files carry BSD 3-clause.
 */

#ifndef HYBX_PLATFORM_H
#define HYBX_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Wire.h>

/* -------------------------------------------------------------------------
 * VL53L5CX_Platform — opaque handle passed through every ULD call.
 * -------------------------------------------------------------------------*/
typedef struct {
    uint16_t  address;   /* 7-bit I2C address (default 0x29) */
    TwoWire  *wire;      /* Wire instance to use (Wire1 on UNO Q QWIIC) */
} VL53L5CX_Platform;

/* -------------------------------------------------------------------------
 * ULD tuning — 1 target per zone, only distance + status output.
 * -------------------------------------------------------------------------*/
#define VL53L5CX_NB_TARGET_PER_ZONE    1U

#define VL53L5CX_DISABLE_AMBIENT_PER_SPAD
#define VL53L5CX_DISABLE_NB_SPADS_ENABLED
#define VL53L5CX_DISABLE_NB_TARGET_DETECTED
#define VL53L5CX_DISABLE_SIGNAL_PER_SPAD
#define VL53L5CX_DISABLE_RANGE_SIGMA_MM
/* VL53L5CX_DISABLE_DISTANCE_MM      — keep */
#define VL53L5CX_DISABLE_REFLECTANCE_PERCENT
/* VL53L5CX_DISABLE_TARGET_STATUS    — keep */
#define VL53L5CX_DISABLE_MOTION_INDICATOR

/* -------------------------------------------------------------------------
 * Mandatory platform functions — implemented in platform.cpp.
 *
 * The ST ULD calls these by the short names RdByte, WrByte, etc.
 * They must have C linkage so the C-compiled ULD can call them from C++.
 * -------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

uint8_t RdByte(VL53L5CX_Platform *p_platform,
               uint16_t RegisterAddress, uint8_t *p_value);

uint8_t WrByte(VL53L5CX_Platform *p_platform,
               uint16_t RegisterAddress, uint8_t value);

uint8_t RdMulti(VL53L5CX_Platform *p_platform,
                uint16_t RegisterAddress,
                uint8_t *p_values, uint32_t size);

uint8_t WrMulti(VL53L5CX_Platform *p_platform,
                uint16_t RegisterAddress,
                uint8_t *p_values, uint32_t size);

void    SwapBuffer(uint8_t *buffer, uint16_t size);

uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs);

#ifdef __cplusplus
}
#endif

#endif /* HYBX_PLATFORM_H */

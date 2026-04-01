# Hybrid RobotiX Parts Inventory

Last updated: March 31, 2026

## Compute Boards

| Board | Qty | Notes |
|-------|-----|-------|
| Arduino UNO Q (4GB) | 1 | Primary SMARS development board |
| Arduino UNO R4 WiFi | 1 | |
| Arduino Nano 33 BLE Sense Rev2 | 1 | |
| Arduino Nano RP2040 Connect | 1 | |
| Arduino Portenta C33 | 1 | |
| Arduino Portenta H7 | 1 | STM32H747XI dual Cortex-M7/M4 |
| Arduino Portenta X8 | 1 | |
| Arduino GIGA R1 WiFi | 1 | STM32H747XI — same processor as Portenta H7 |
| Adafruit Feather RP2350 with PSRAM and HSTX connector | 1 | |
| Raspberry Pi 4B 8GB | 8 | Potential cluster — 32 cores / 64GB RAM total |
| Raspberry Pi 5 | 1 | Bad USB port — Ham System currently non-functional |

## Sensors

| Sensor | Qty | Notes |
|--------|-----|-------|
| Adafruit SCD30 (CO2/temp/humidity) | 1 | Working — scd30-app example in this repo |
| Adafruit BNO055 (9-DoF absolute orientation) | 1 | Planned for SMARS |
| Adafruit LSM6DSOX (6-DoF IMU accel/gyro) | 2 | Working — imu-app example in this repo |

## Breakout Boards

| Board | Qty | Notes |
|-------|-----|-------|
| Adafruit Infineon Trust M (ADA4351) | 2 | Crypto authentication — ECC, RSA, TRNG, STEMMA QT — planned for SecureBot end-to-end encrypted MQTT |
| Adafruit HSTX Breakout Board | 1 | Companion to Feather RP2350 |

## Shields / HATs

| Item | Qty | Notes |
|------|-----|-------|
| Adafruit Motor Shield V2 | 1 | Arriving Thursday — for SMARS motor control |
| Arduino GIGA Display Shield | 1 | Touchscreen UI for GIGA R1 |

## Chassis / Mechanical

| Item | Qty | Notes |
|------|-----|-------|
| SMARS Chassis | 1 | 3D printed — awaiting N20 motors and wheels |

## On Order

| Item | ETA | Notes |
|------|-----|-------|
| Adafruit Motor Shield V2 | Thursday April 2nd | For SMARS |
| Adafruit LIS3DH | April 5th-9th | Accelerometer — repo example planned |
| USB Power Switch | Thursday April 2nd | |
| N20 150:1 motors with encoders x4 | TBD | Awaiting funds — ~$70 |
| N20 compatible wheels x4 | TBD | Awaiting funds — included in above |

## Wanted / Planned

| Item | Notes |
|------|-------|
| Adafruit ENS161 | MOX gas sensor — TVOC, eCO2, AQI |
| Adafruit BME688 | Temp, humidity, pressure, VOC |
| SparkFun VL53L5CX | 8x8 ToF depth sensor — DOA, RMA submitted |
| VL53L1X | Long range ToF distance sensor |
| N20 150:1 motors with encoders x4 | SMARS drivetrain |

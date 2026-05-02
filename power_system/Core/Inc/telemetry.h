/*
 * telemetry.h
 *
 * Public interface for the CAN telemetry module.
 * 
 * Reads all five INA228 sensors, applies a rolling average to voltage and
 * current via circular_buffer, then packs the results into CAN frames.
 * 
 * To disable a sensor during testing:
 * Set its entry in SENSOR_ENABLED to 0. Order: { BUS, M1, M2, M3, M4 }
 */

#ifndef INC_TELEMETRY_H_
#define INC_TELEMETRY_H_

#include "main.h"
#include "precharge.h"
#include "ina228_driver.h"
#include "circular_buffer.h"
#include <stdint.h>
#include <stdbool.h>

// CAN IDs
#define CAN_ID_BUS      0x100
#define CAN_ID_MOTOR1   0x101
#define CAN_ID_MOTOR2   0x102
#define CAN_ID_MOTOR3   0x103
#define CAN_ID_MOTOR4   0x104

// Sensors for testing
#define NUM_SENSORS     5
#define SENSOR_ENABLED  { 1, 1, 1, 1, 1 }   // Order: BUS, M1, M2, M3, M4


// Public API Functions
void telemetry_init(void);
void telemetry_tick(void);

#endif /* INC_TELEMETRY_H_ */

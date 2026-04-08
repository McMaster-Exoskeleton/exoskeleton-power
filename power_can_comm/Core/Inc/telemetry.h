/*
 * telemetry.h
 *
 *  Created on: Apr 4, 2026
 *      Author: EmChan
 *
 *  Reads all five INA228 sensors, applies a rolling average to voltage and
 *  current via circular_buffer, then packs the results into CAN frames.
 *
 *  Motor and bus have separate payload structs because they carry different
 *  status fields (relay_status vs contactor_status, system_fault on bus only).
 *
 * ── To disable a sensor during testing ───────────────────────────────────
 *  Set its entry in SENSOR_ENABLED to 0. Order: { BUS, M1, M2, M3, M4 }
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
// STM32 sends 5 frames sequentially, each with a different ID through the same CAN transceiver
// Dashboard will sensor data based on ID


// Sensors for testing
#define NUM_SENSORS     5
#define SENSOR_ENABLED  { 1, 0, 0, 0, 0 }   // Order: BUS, M1, M2, M3, M4

// JSON Payload Formats

typedef struct {
    float voltage;          // Rolling-average voltage (V)
    float current;          // Rolling-average current (A)
    bool  relay_status;     // True = relay closed
    bool  sensor_status;    // True = INA228 healthy
} MotorPayload_t;

typedef struct {
    float voltage;          // Rolling-average voltage (V)
    float current;          // Rolling-average current (A)
    bool  contactor_status; // True = contactor closed (Precharge FSM in STATE_NORMAL_OPERATION)
    bool  sensor_status;    // True = INA228 healthy
    bool  system_fault;     // True = active fault (detected by FSM)
} BusPayload_t;

// Public API Functions

// Call once after precharge_control_init()
void telemetry_init(void);

// Call periodically — updates rolling averages and transmits CAN frames
void telemetry_tick(void);

#endif /* INC_TELEMETRY_H_ */

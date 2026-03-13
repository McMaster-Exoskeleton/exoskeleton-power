/*
 * precharge.h
 *
 *  Created on: Feb 15, 2026
 *      Author: EmChan
 */

#ifndef INC_PRECHARGE_H_
#define INC_PRECHARGE_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"
#include "ina228_driver.h"

/* FSM States */
typedef enum {
    STATE_PRECHARGE,
    STATE_NORMAL_OPERATION,
    STATE_FAULT,
} PrechargeState_t;

/* Fault Types */
typedef enum {
    FAULT_NONE = 0,
	FAULT_MOTOR_OVERCURRENT,
    FAULT_BUS_OVERCURRENT,
    FAULT_BUS_OVERVOLTAGE,
    FAULT_BUS_UNDERVOLTAGE,
    FAULT_SENSOR_COMM,
} FaultType_t;

/* INA228 Measurement Locations */
typedef enum {
    INA228_MOTOR1,
	INA228_MOTOR2,
	INA228_MOTOR3,
	INA228_MOTOR4,
	INA228_BUS
} INA228_Location_t;

/* Sensor Data Structure */
typedef struct {
    float voltage;           // Voltage in V
    float current;           // Current in A
    float power;             // Power in W
    uint8_t healthy;         // Sensor health flag (1 = healthy, 0 = fault/comm error)
} SensorData_t;

/* System Status Structure */
typedef struct {
    PrechargeState_t state;
    FaultType_t fault;
    SensorData_t motor1_sensor;
    SensorData_t motor2_sensor;
    SensorData_t motor3_sensor;
    SensorData_t motor4_sensor;
    SensorData_t bus_sensor;
} SystemStatus_t;

/* Expose global system status so main.c can read sensor data directly */
extern SystemStatus_t g_system_status;

/* Configuration Parameters */
#define PRECHARGE_THRESHOLD_PERCENT 95      // Bus voltage must reach 95% of battery
#define SENSOR_POLL_INTERVAL_MS     50      // Poll sensors every 50ms

#define BUS_OVERVOLTAGE_THRESHOLD   56.0f   // 56V overvoltage threshold for bus
#define BUS_UNDERVOLTAGE_THRESHOLD  40.0f   // 40V undervoltage threshold for bus
#define BATTERY_NOMINAL				48.0f 	// 48V nominal battery for system

/* CAN Message IDs */
#define CAN_ID_MOTOR1   0x101
#define CAN_ID_MOTOR2   0x102
#define CAN_ID_MOTOR3   0x103
#define CAN_ID_MOTOR4   0x104
#define CAN_ID_BUS      0x105

/* UART Data Logging */
#define RX_BUF_SIZE     64
#define MAX_SAMPLES     2000

/* Function Prototypes */
void precharge_control_init(void);
void precharge_fsm_tick(void);
PrechargeState_t get_current_state(void);
FaultType_t get_current_fault(void);
void get_sensor_data(INA228_Location_t location, SensorData_t* data);

#endif /* INC_PRECHARGE_H_ */

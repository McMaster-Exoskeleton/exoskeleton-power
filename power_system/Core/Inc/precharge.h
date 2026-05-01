/*
 * precharge.h
 *
 * Public interface for the precharge FSM and system status module.
 * Defines FSM state and fault codes, the INA228 sensor location enum,
 * per-sensor and system-wide status structures, threshold constants for
 * voltage and current protection, and the public API functions used by
 * main.c and telemetry.c to query system state and sensor readings.
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
    FAULT_NONE,
    FAULT_BUS_OVERCURRENT,
    FAULT_BUS_OVERVOLTAGE,
    FAULT_BUS_UNDERVOLTAGE,
	FAULT_MOTOR_OVERCURRENT
} FaultType_t;

/* INA228 Measurement Locations */
typedef enum {
	INA228_BUS,
    INA228_MOTOR1,
	INA228_MOTOR2,
	INA228_MOTOR3,
	INA228_MOTOR4
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
    SensorData_t bus_sensor;
    SensorData_t motor1_sensor;
    SensorData_t motor2_sensor;
    SensorData_t motor3_sensor;
    SensorData_t motor4_sensor;
} SystemStatus_t;

/* Expose global system status so main.c can read sensor data directly */
extern SystemStatus_t g_system_status;

/* Configuration Parameters */
#define PRECHARGE_THRESHOLD_PERCENT 90      // Bus voltage must reach (Threshold)% of battery nominal
#define SENSOR_POLL_INTERVAL_MS     50      // Poll sensors every 50ms

#define BUS_OVERVOLTAGE_THRESHOLD   48.0f   // Overvoltage threshold for bus
#define BUS_UNDERVOLTAGE_THRESHOLD  30.0f    // Undervoltage threshold for bus
#define BATTERY_NOMINAL				40.0f   // Nominal battery for system

#define BUS_OVERCURRENT_THRESHOLD	50.0f   // Overcurrent threshold for bus
#define MOTOR_OVERCURRENT_THRESHOLD	25.0f   // Overcurrent threshold for motors

/* Function Prototypes */
void precharge_control_init(void);
void precharge_fsm_tick(void);
PrechargeState_t get_current_state(void);
FaultType_t get_current_fault(void);
void get_sensor_data(INA228_Location_t location, SensorData_t* data);

#endif /* INC_PRECHARGE_H_ */

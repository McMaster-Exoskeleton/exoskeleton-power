/*
 * precharge.c
 *
 *  Created on: Feb 15, 2026
 *      Author: EmChan
 */

#include "precharge.h"
#include "ina228_driver.h"
#include "gpio.h"

/* Global system status */
static SystemStatus_t g_system_status = {0};
static uint32_t last_sensor_poll_time = 0;

/* Pin definitions */
#define CONTACTOR_PORT         	GPIOA
#define CONTACTOR_PIN       	GPIO_PIN_0
#define MOTOR_PORT         		GPIOC
#define MOTOR1_PIN     			GPIO_PIN_0
#define MOTOR2_PIN     			GPIO_PIN_1
#define MOTOR3_PIN     			GPIO_PIN_2
#define MOTOR4_PIN     			GPIO_PIN_3


/* Local Prototypes */
static void FSM_Precharge(void);
static void FSM_Normal_Operation(void);
static void FSM_Fault(void);
static void UpdateSensorReadings(void);
static uint8_t IsPrechargeComplete(void);
static uint8_t CheckForFaults(void);
static void SetContactor(uint8_t on);
static void PowerMotors(uint8_t on);

/* Initialize precharge control system */
void precharge_control_init(void) {
    // Initialize system status
    g_system_status.state = STATE_PRECHARGE;
    g_system_status.fault = FAULT_NONE;

    // Open contactor
    SetContactor(0);

    // Initialize INA228 sensors
    HAL_StatusTypeDef status;

    // Motor1 sensor
    status = INA228_Init(INA228_ADDR1, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.motor1_sensor.healthy = 0;
    } else {
        g_system_status.motor1_sensor.healthy = 1;
        // Configure motor overcurrent threshold (over/under voltage not applicable)
        INA228_ConfigureAlerts(INA228_ADDR1, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
    }

    // Motor2 sensor
    status = INA228_Init(INA228_ADDR2, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.motor2_sensor.healthy = 0;
    } else {
        g_system_status.motor2_sensor.healthy = 1;
        // Configure motor overcurrent threshold (over/under voltage not applicable)
        INA228_ConfigureAlerts(INA228_ADDR2, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
    }

    // Motor3 sensor
    status = INA228_Init(INA228_ADDR1, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.motor3_sensor.healthy = 0;
    } else {
        g_system_status.motor3_sensor.healthy = 1;
        // Configure motor overcurrent threshold (over/under voltage not applicable)
        INA228_ConfigureAlerts(INA228_ADDR3, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
    }

    // Motor4 sensor
    status = INA228_Init(INA228_ADDR4, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.motor4_sensor.healthy = 0;
    } else {
        g_system_status.motor4_sensor.healthy = 1;
        // Configure motor overcurrent threshold (over/under voltage not applicable)
        INA228_ConfigureAlerts(INA228_ADDR4, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
    }

    // Bus sensor
    status = INA228_Init(INA228_ADDR5, BUS_CURRENT_LSB, BUS_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.bus_sensor.healthy = 0;
    } else {
        g_system_status.bus_sensor.healthy = 1;
        // Motor bus: current threshold only (25A), no voltage limits
        INA228_ConfigureAlerts(INA228_ADDR5, BUS_SHUNT_RESISTOR, BUS_OVERVOLTAGE_THRESHOLD, BUS_UNDERVOLTAGE_THRESHOLD, BUS_OVERCURRENT_THRESHOLD);
    }

    // Initial sensor reading
    UpdateSensorReadings();
}

/* Main FSM tick function */
void precharge_fsm_tick(void) {
    // Update sensor readings periodically
    uint32_t current_time = HAL_GetTick();
    if (current_time - last_sensor_poll_time >= SENSOR_POLL_INTERVAL_MS) {
        UpdateSensorReadings();
        last_sensor_poll_time = current_time;
    }

    // Execute state machine
    switch (g_system_status.state) {
        case STATE_PRECHARGE:
            FSM_Precharge();
            break;
        case STATE_NORMAL_OPERATION:
            FSM_Normal_Operation();
            break;
        case STATE_FAULT:
            FSM_Fault();
            break;
        default:
            // Invalid state - enter fault
            g_system_status.state = STATE_FAULT;
            g_system_status.fault = FAULT_NONE;
            break;
    }
}


/* FSM State: PRECHARGE */
static void FSM_Precharge(void) {
    // Keep contactor open - precharge happens through parallel resistor
    SetContactor(0);
    // No power to motors
    PowerMotors(0);

    // Check for faults
    if (CheckForFaults()) {
        g_system_status.state = STATE_FAULT;
        return;
    }

    // Check if precharge is complete
    if (IsPrechargeComplete()) {
        g_system_status.state = STATE_NORMAL_OPERATION;

    }
}

/* FSM State: NORMAL OPERATION */
static void FSM_Normal_Operation(void) {
    // Maintain contactor closed
    SetContactor(1);
    // Send power to motors
    PowerMotors(1);
}


/* FSM State: FAULT */
static void FSM_Fault(void) {
    // Contactor open
    SetContactor(0);
    // No power to motors
    PowerMotors(0);
}

/* Update all sensor readings */
static void UpdateSensorReadings(void) {

    if (g_system_status.motor1_sensor.healthy) {
        if (INA228_ReadVoltage(INA228_ADDR1, &g_system_status.motor1_sensor.voltage) != HAL_OK) {
            g_system_status.motor1_sensor.healthy = 0;
        }
        if (INA228_ReadCurrent(INA228_ADDR1, &g_system_status.motor1_sensor.current, MOTOR_CURRENT_LSB) != HAL_OK) {
        	g_system_status.motor1_sensor.healthy = 0;
        }
        if (INA228_ReadPower(INA228_ADDR1, &g_system_status.motor1_sensor.power, MOTOR_POWER_LSB) != HAL_OK) {
        	g_system_status.motor1_sensor.healthy = 0;
        }
    }

    if (g_system_status.motor2_sensor.healthy) {
        if (INA228_ReadVoltage(INA228_ADDR2, &g_system_status.motor2_sensor.voltage) != HAL_OK) {
            g_system_status.motor2_sensor.healthy = 0;
        }
        if (INA228_ReadCurrent(INA228_ADDR2, &g_system_status.motor2_sensor.current, MOTOR_CURRENT_LSB) != HAL_OK) {
        	g_system_status.motor2_sensor.healthy = 0;
        }
        if (INA228_ReadPower(INA228_ADDR2, &g_system_status.motor2_sensor.power, MOTOR_POWER_LSB) != HAL_OK) {
        	g_system_status.motor2_sensor.healthy = 0;
        }
    }

    if (g_system_status.motor3_sensor.healthy) {
        if (INA228_ReadVoltage(INA228_ADDR3, &g_system_status.motor3_sensor.voltage) != HAL_OK) {
            g_system_status.motor3_sensor.healthy = 0;
        }
        if (INA228_ReadCurrent(INA228_ADDR3, &g_system_status.motor3_sensor.current, MOTOR_CURRENT_LSB) != HAL_OK) {
        	g_system_status.motor3_sensor.healthy = 0;
        }
        if (INA228_ReadPower(INA228_ADDR3, &g_system_status.motor3_sensor.power, MOTOR_POWER_LSB) != HAL_OK) {
        	g_system_status.motor3_sensor.healthy = 0;
        }
    }

    if (g_system_status.motor4_sensor.healthy) {
        if (INA228_ReadVoltage(INA228_ADDR4, &g_system_status.motor4_sensor.voltage) != HAL_OK) {
            g_system_status.motor4_sensor.healthy = 0;
        }
        if (INA228_ReadCurrent(INA228_ADDR4, &g_system_status.motor4_sensor.current, MOTOR_CURRENT_LSB) != HAL_OK) {
        	g_system_status.motor4_sensor.healthy = 0;
        }
        if (INA228_ReadPower(INA228_ADDR4, &g_system_status.motor4_sensor.power, MOTOR_POWER_LSB) != HAL_OK) {
        	g_system_status.motor4_sensor.healthy = 0;
        }
    }

    if (g_system_status.bus_sensor.healthy) {
        if (INA228_ReadVoltage(INA228_ADDR5, &g_system_status.bus_sensor.voltage) != HAL_OK) {
            g_system_status.bus_sensor.healthy = 0;
        }
        if (INA228_ReadCurrent(INA228_ADDR5, &g_system_status.bus_sensor.current, BUS_CURRENT_LSB) != HAL_OK) {
        	g_system_status.bus_sensor.healthy = 0;
        }
        if (INA228_ReadPower(INA228_ADDR5, &g_system_status.bus_sensor.power, BUS_POWER_LSB) != HAL_OK) {
        	g_system_status.bus_sensor.healthy = 0;
        }
    }
}

/* Check for fault conditions */
static uint8_t CheckForFaults(void) {

    // Check sensor communication
    if (!g_system_status.motor1_sensor.healthy ||
    	!g_system_status.motor2_sensor.healthy ||
		!g_system_status.motor3_sensor.healthy ||
		!g_system_status.motor4_sensor.healthy ||
        !g_system_status.bus_sensor.healthy)
    {
        g_system_status.fault = FAULT_SENSOR_COMM;
        return 1;
    }

    // Check motor overcurrent
    if (g_system_status.motor1_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
    	g_system_status.motor2_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
		g_system_status.motor3_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
		g_system_status.motor4_sensor.current > MOTOR_OVERCURRENT_THRESHOLD) {
        g_system_status.fault = FAULT_MOTOR_OVERCURRENT;
        return 1;
    }

    // Check bus overcurrent
    if (g_system_status.bus_sensor.current > BUS_OVERCURRENT_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_OVERCURRENT;
        return 1;
    }

    // Check bus overvoltage
    if (g_system_status.bus_sensor.voltage > BUS_OVERVOLTAGE_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_OVERVOLTAGE;
        return 1;
    }

    // Check bus undervoltage
    if (g_system_status.bus_sensor.voltage > BUS_UNDERVOLTAGE_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_UNDERVOLTAGE;
        return 1;
    }

    return 0;
}

/* Check if precharge is complete */
static uint8_t IsPrechargeComplete(void) {
    // Bus voltage should reach 95% of battery voltage
    return (g_system_status.bus_sensor.voltage >= (BATTERY_NOMINAL * ((float)PRECHARGE_THRESHOLD_PERCENT / 100.0f)));
}

/* Energize contactor */
static void SetContactor(uint8_t on) {
    if (on) {
        HAL_GPIO_WritePin(CONTACTOR_PORT, CONTACTOR_PIN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(CONTACTOR_PORT, CONTACTOR_PIN, GPIO_PIN_RESET);
    }
}

/* Send power to motors */
static void PowerMotors(uint8_t on) {
    if (on) {
    	// Motors active LO
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR2_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR3_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR4_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR2_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR3_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_PORT, MOTOR4_PIN, GPIO_PIN_SET);
    }
}

/* Public API functions */
PrechargeState_t get_current_state(void) {
    return g_system_status.state;
}

FaultType_t get_current_fault(void) {
    return g_system_status.fault;
}

void get_sensor_data(INA228_Location_t location, SensorData_t* data) {
    if (data == NULL) return;

    switch (location) {
        case INA228_MOTOR1:
            *data = g_system_status.motor1_sensor;
            break;
        case INA228_MOTOR2:
            *data = g_system_status.motor2_sensor;
            break;
        case INA228_MOTOR3:
            *data = g_system_status.motor3_sensor;
            break;
        case INA228_MOTOR4:
            *data = g_system_status.motor4_sensor;
            break;
        case INA228_BUS:
            *data = g_system_status.bus_sensor;
            break;
        default:
            break;
    }
}


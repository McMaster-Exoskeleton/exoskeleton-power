/*
 * precharge.c
 *
 * Precharge finite state machine (FSM) and system fault management.
 * Implements three states — PRECHARGE, NORMAL_OPERATION, and FAULT — to
 * safely ramp bus voltage before closing the main contactor. Polls all five
 * INA228 sensors (1 bus + 4 motors) on a configurable interval, enforces
 * over/undervoltage and overcurrent thresholds, and controls the contactor
 * and motor relay GPIO pins. Faults are latched until an external reset.
 */


#include "precharge.h"
#include "ina228_driver.h"
#include "gpio.h"

/* Global System Status */ 
SystemStatus_t g_system_status  = {0};
uint32_t last_sensor_poll_time  = 0;

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
    g_system_status.state = STATE_PRECHARGE;
    g_system_status.fault = FAULT_NONE;

    SetContactor(0); // Contactor open at startup

    HAL_StatusTypeDef status;

    // Initialize Bus sensor
    status = INA228_Init(INA228_ADDR1, BUS_CURRENT_LSB, BUS_SHUNT_RESISTOR);
    if (status != HAL_OK) {
        g_system_status.bus_sensor.healthy = 0;
    } else {
        g_system_status.bus_sensor.healthy = 1;
        INA228_ConfigureAlerts(INA228_ADDR1, BUS_SHUNT_RESISTOR, BUS_OVERVOLTAGE_THRESHOLD, BUS_UNDERVOLTAGE_THRESHOLD, BUS_OVERCURRENT_THRESHOLD);
    }

    
    // Initialize Motor1 sensor
	status = INA228_Init(INA228_ADDR2, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
	if (status != HAL_OK) {
		g_system_status.motor1_sensor.healthy = 0;
	} else {
		g_system_status.motor1_sensor.healthy = 1;
		// Configure motor overcurrent threshold (over/under voltage not applicable due to backfeed)
		INA228_ConfigureAlerts(INA228_ADDR2, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
	}

	// Initialize Motor2 sensor
	status = INA228_Init(INA228_ADDR3, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
	if (status != HAL_OK) {
		g_system_status.motor2_sensor.healthy = 0;
	} else {
		g_system_status.motor2_sensor.healthy = 1;
		// Configure motor overcurrent threshold (over/under voltage not applicable due to backfeed)
		INA228_ConfigureAlerts(INA228_ADDR3, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
	}

	// Initialize Motor3 sensor
	status = INA228_Init(INA228_ADDR4, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
	if (status != HAL_OK) {
		g_system_status.motor3_sensor.healthy = 0;
	} else {
		g_system_status.motor3_sensor.healthy = 1;
		// Configure motor overcurrent threshold (over/under voltage not applicable due to backfeed)
		INA228_ConfigureAlerts(INA228_ADDR4, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
	}

	// Initialize Motor4 sensor
	status = INA228_Init(INA228_ADDR5, MOTOR_CURRENT_LSB, MOTOR_SHUNT_RESISTOR);
	if (status != HAL_OK) {
		g_system_status.motor4_sensor.healthy = 0;
	} else {
		g_system_status.motor4_sensor.healthy = 1;
		// Configure motor overcurrent threshold (over/under voltage not applicable due to backfeed)
		INA228_ConfigureAlerts(INA228_ADDR5, MOTOR_SHUNT_RESISTOR, 100.0f, 0.0f, MOTOR_OVERCURRENT_THRESHOLD);
	}
	
    UpdateSensorReadings(); // Take initial sensor readings
}

/* Main FSM tick function */
void precharge_fsm_tick(void) {

    // Poll sensors on interval
    uint32_t now = HAL_GetTick();
    if (now - last_sensor_poll_time >= SENSOR_POLL_INTERVAL_MS) {
        UpdateSensorReadings();
        last_sensor_poll_time = now;
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
            // Invalid state - safe fallback
            g_system_status.state = STATE_FAULT;
            g_system_status.fault = FAULT_NONE;
            break;
    }
}


/* FSM State: PRECHARGE */
static void FSM_Precharge(void) {
    SetContactor(0); // Contactor open - precharge happens through parallel resistor
    PowerMotors(0);  // Motor relays open

    if(CheckForFaults()) {
        g_system_status.state = STATE_FAULT;
        return;
    }

    if(IsPrechargeComplete()) {
        g_system_status.state = STATE_NORMAL_OPERATION;
    }
}

/* FSM State: NORMAL OPERATION */
static void FSM_Normal_Operation(void) {
    SetContactor(1);     // Contactor closed - current flow bypasses precharge resistor
    PowerMotors(1); 	 // Motor relays closed

    if(CheckForFaults()) {
        g_system_status.state = STATE_FAULT;
    }
}


/* FSM State: FAULT */
static void FSM_Fault(void) {
    SetContactor(0); // Contactor open
    PowerMotors(0);  // Motor relays open

    // NOTE: Fault is latched until external reset
}

/* Update all sensor readings from INA228s */
static void UpdateSensorReadings(void) {

    // Verify I2C communication and sensor health each cycle
    uint8_t healthy = 0;
    HAL_StatusTypeDef status;

    // Bus
    status = INA228_CheckHealth(INA228_ADDR1, &healthy);
    if (status == HAL_OK && healthy){
        g_system_status.bus_sensor.healthy = 1;
        // Sensor readings
        if (INA228_ReadVoltage(INA228_ADDR1, &g_system_status.bus_sensor.voltage) != HAL_OK ||
            INA228_ReadCurrent(INA228_ADDR1, &g_system_status.bus_sensor.current, BUS_CURRENT_LSB) != HAL_OK ||
            INA228_ReadPower(INA228_ADDR1, &g_system_status.bus_sensor.power, BUS_POWER_LSB) != HAL_OK){
            	g_system_status.bus_sensor.healthy = 0;
        }
    }
    else {
        g_system_status.bus_sensor.healthy = 0;
    }

    
    // Motor 1
    status = INA228_CheckHealth(INA228_ADDR2, &healthy);
    if(status == HAL_OK && healthy){
    	g_system_status.motor1_sensor.healthy = 1;
		// Sensor readings
		if (INA228_ReadVoltage(INA228_ADDR2, &g_system_status.motor1_sensor.voltage) != HAL_OK ||
			INA228_ReadCurrent(INA228_ADDR2, &g_system_status.motor1_sensor.current, BUS_CURRENT_LSB) != HAL_OK ||
			INA228_ReadPower(INA228_ADDR2, &g_system_status.motor1_sensor.power, BUS_POWER_LSB) != HAL_OK){
				g_system_status.motor1_sensor.healthy = 0;
		}
	}
    else {
		g_system_status.motor1_sensor.healthy = 0;
	}

    // Motor 2
    status = INA228_CheckHealth(INA228_ADDR3, &healthy);
    if(status == HAL_OK && healthy){
    	g_system_status.motor2_sensor.healthy = 1;
		// Sensor readings
		if (INA228_ReadVoltage(INA228_ADDR3, &g_system_status.motor2_sensor.voltage) != HAL_OK ||
			INA228_ReadCurrent(INA228_ADDR3, &g_system_status.motor2_sensor.current, BUS_CURRENT_LSB) != HAL_OK ||
			INA228_ReadPower(INA228_ADDR3, &g_system_status.motor2_sensor.power, BUS_POWER_LSB) != HAL_OK){
				g_system_status.motor2_sensor.healthy = 0;
		}
	}
    else {
		g_system_status.motor2_sensor.healthy = 0;
	}

    // Motor 3
    status = INA228_CheckHealth(INA228_ADDR4, &healthy);
    if(status == HAL_OK && healthy){
    	g_system_status.motor3_sensor.healthy = 1;
		// Sensor readings
		if (INA228_ReadVoltage(INA228_ADDR4, &g_system_status.motor3_sensor.voltage) != HAL_OK ||
			INA228_ReadCurrent(INA228_ADDR4, &g_system_status.motor3_sensor.current, BUS_CURRENT_LSB) != HAL_OK ||
			INA228_ReadPower(INA228_ADDR4, &g_system_status.motor3_sensor.power, BUS_POWER_LSB) != HAL_OK){
				g_system_status.motor3_sensor.healthy = 0;
		}
	}
    else {
		g_system_status.motor3_sensor.healthy = 0;
	}

    // Motor 4
    status = INA228_CheckHealth(INA228_ADDR5, &healthy);
    if(status == HAL_OK && healthy){
    	g_system_status.motor4_sensor.healthy = 1;
		// Sensor readings
		if (INA228_ReadVoltage(INA228_ADDR5, &g_system_status.motor4_sensor.voltage) != HAL_OK ||
			INA228_ReadCurrent(INA228_ADDR5, &g_system_status.motor4_sensor.current, BUS_CURRENT_LSB) != HAL_OK ||
			INA228_ReadPower(INA228_ADDR5, &g_system_status.motor4_sensor.power, BUS_POWER_LSB) != HAL_OK){
				g_system_status.motor4_sensor.healthy = 0;
		}
	}
    else {
		g_system_status.motor4_sensor.healthy = 0;
	}
	
}

/* Check for fault conditions */
static uint8_t CheckForFaults(void) {

	
    // Motor overcurrent
    if (g_system_status.motor1_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
    	g_system_status.motor2_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
		g_system_status.motor3_sensor.current > MOTOR_OVERCURRENT_THRESHOLD ||
		g_system_status.motor4_sensor.current > MOTOR_OVERCURRENT_THRESHOLD) {
        g_system_status.fault = FAULT_MOTOR_OVERCURRENT;
        return 1;
    }
    

    // Bus overcurrent
    if (g_system_status.bus_sensor.current > BUS_OVERCURRENT_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_OVERCURRENT;
        return 1;
    }

    // Bus overvoltage
    if (g_system_status.bus_sensor.voltage > BUS_OVERVOLTAGE_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_OVERVOLTAGE;
        return 1;
    }

    // Bus undervoltage (only meaningful during normal operation, bus starts low during precharge)
    if (g_system_status.state == STATE_NORMAL_OPERATION && g_system_status.bus_sensor.voltage < BUS_UNDERVOLTAGE_THRESHOLD) {
        g_system_status.fault = FAULT_BUS_UNDERVOLTAGE;
        return 1;
    }

    return 0;
}

/* Check if precharge is complete */
static uint8_t IsPrechargeComplete(void) {
    // Bus voltage must reach PRECHARGE_THRESHOLD_PERCENT of nominal
    return (g_system_status.bus_sensor.voltage >= (BATTERY_NOMINAL * ((float)PRECHARGE_THRESHOLD_PERCENT / 100.0f)));
}

/* Contactor control */
static void SetContactor(uint8_t on) {
    if(on) 	HAL_GPIO_WritePin(CONTACTOR_PORT, CONTACTOR_PIN, GPIO_PIN_SET);
    else 	HAL_GPIO_WritePin(CONTACTOR_PORT, CONTACTOR_PIN, GPIO_PIN_RESET);
}

/* Motor control */
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
        case INA228_BUS:
            *data = g_system_status.bus_sensor;
            break;
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
        default:
            break;
    }
}


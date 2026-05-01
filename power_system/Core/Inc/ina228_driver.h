/*
 * ina228_driver.h
 *
 * Public interface for the INA228 power monitor driver.
 * Defines I2C device addresses, register map, configuration bit masks,
 * calibration constants for the bus and motor sensor locations, and LSB
 * scaling values used for voltage, current, power, and alert registers.
 * Declares all driver API functions (init, read, health check, alerts).
 */

#ifndef INC_INA228_DRIVER_H_
#define INC_INA228_DRIVER_H_

#include "main.h"
#include "i2c.h"

/* INA228 I2C Addresses */
// NOTE: must shift device address left 1 bit because STM32 HAL expects 8-bit address format (7-bit address frame + 1 R/W bit)
#define INA228_ADDR1     (0x40 << 1)   // A1=GND, A0=GND // Bus
#define INA228_ADDR2     (0x41 << 1)   // A1=GND, A0=VCC // Motor1
#define INA228_ADDR3     (0x42 << 1)   // A1=GND, A0=SDA // Motor2
#define INA228_ADDR4     (0x43 << 1)   // A1=GND, A0=SCL // Motor3
#define INA228_ADDR5     (0x44 << 1)   // A1=VCC, A0=GND // Motor4

/* INA228 Register Addresses */
#define INA228_REG_CONFIG       	0x00
#define INA228_REG_ADC_CONFIG   	0x01
#define INA228_REG_SHUNT_CAL    	0x02
#define INA228_REG_SHUNT_TEMPCO 	0x03
#define INA228_REG_VSHUNT       	0x04
#define INA228_REG_VBUS         	0x05
#define INA228_REG_DIETEMP      	0x06
#define INA228_REG_CURRENT      	0x07
#define INA228_REG_POWER        	0x08
#define INA228_REG_ENERGY       	0x09
#define INA228_REG_CHARGE       	0x0A
#define INA228_REG_DIAG_ALRT    	0x0B
#define INA228_REG_SOVL         	0x0C
#define INA228_REG_SUVL         	0x0D
#define INA228_REG_BOVL         	0x0E
#define INA228_REG_BUVL        	 	0x0F
#define INA228_REG_TEMP_LIMIT   	0x10
#define INA228_REG_PWR_LIMIT    	0x11
#define INA228_REG_MANUFACTURER_ID 	0x3E // Should be 0x5449
#define INA228_REG_DEVICE_ID   	 	0x3F

/* Configuration Bits */
#define INA228_CONFIG_RST       (1 << 15) 	// software reset bit
#define INA228_CONFIG_CONVDLY_0 (0 << 6)  	// conversion delay time = 0ms
#define INA228_CONFIG_ADCRANGE  (0 << 4)  	// ±163.84 mV shunt measurement range

/* ADC Configuration */
#define INA228_ADC_MODE_CONT_ALL 	(0x0F << 12) 	// mode = continuous for all measurements
#define INA228_ADC_VBUSCT_1052us	(0x05 << 9) 	// bus voltage conversion time = 1.052ms
#define INA228_ADC_VSHCT_1052us 	(0x05 << 6)		// shunt voltage conversion time = 1.052ms
#define INA228_ADC_VTCT_1052us 		(0x05 << 3)		// temperature conversion time = 1.052ms
#define INA228_ADC_AVG_64   		(0x03 << 0)		// average = 64 individual measurements

/* Bus Calibration Constants */
#define BUS_SHUNT_RESISTOR    			0.003f  	// 3 mOhm (calculations in Power Task 4 doc)
#define BUS_CURRENT_MAX					50.0f		// 50A max current
#define BUS_CURRENT_LSB  				(BUS_CURRENT_MAX / 524288.0f)   	// datasheet Section 8.1.2 equation 3
#define BUS_POWER_LSB					(3.2f * BUS_CURRENT_LSB) 					// datasheet Section 8.1.2 equation 5

/* Motor Calibration Constants */
#define MOTOR_SHUNT_RESISTOR    		0.006f  	// 6 mOhm (calculations in Power Task 4 doc)
#define MOTOR_CURRENT_MAX   			25.0f   	// 25A max current
#define MOTOR_CURRENT_LSB  				(MOTOR_CURRENT_MAX / 524288.0f)
#define MOTOR_POWER_LSB					(3.2f * MOTOR_CURRENT_LSB)

/* Shared Calibration Constants */
#define INA228_VBUS_LSB			0.0001953125f 	// 195.3125 uV per LSB (datasheet Table 8-1)
#define INA228_BOVL_LSB			0.003125f 		// 3.125 mV per LSB (datasheet Section 7.6.1.15)
#define INA228_BUVL_LSB			0.003125f 		// 3.125 mV per LSB (datasheet Section 7.6.1.16)
#define INA228_SOVL_LSB			0.000005f	    // 5uV per LSB when ADCRANGE = 0 (datasheet Section 7.6.1.13)

/* Function Prototypes */
HAL_StatusTypeDef INA228_Init(uint8_t device_addr, float current_LSB, float shunt_resistor);
HAL_StatusTypeDef INA228_ReadManufacturerID(uint8_t device_addr, uint16_t *id);					// Call this to verify I2C communication
HAL_StatusTypeDef INA228_ReadVoltage(uint8_t device_addr, float* voltage);
HAL_StatusTypeDef INA228_ReadCurrent(uint8_t device_addr, float* current, float current_LSB);
HAL_StatusTypeDef INA228_ReadPower(uint8_t device_addr, float* power, float power_LSB);
HAL_StatusTypeDef INA228_CheckHealth(uint8_t device_addr, uint8_t* healthy);
HAL_StatusTypeDef INA228_ConfigureAlerts(uint8_t device_addr, float shunt_resistor, float overvoltage_limit, float undervoltage_limit, float overcurrent_limit);

#endif /* INC_INA228_DRIVER_H_ */


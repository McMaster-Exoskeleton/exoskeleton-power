/*
 * ina228.driver.c
 *
 *  Created on: Nov 21, 2025
 *      Author: EmChan
 */

#include "ina228_driver.h"
#include <math.h>

/* I2C Timeout */
#define INA228_I2C_TIMEOUT  100  // 100ms timeout

// NOTE: INA228 sends data in big-endian (MSB = lowest mem address), but STM32 stores data in little-endian (LSB = lowest mem address)
// Must manually shift/ control byte order to account for different platform endianness

/* Helper function to write 16-bit register */
static HAL_StatusTypeDef INA228_WriteRegister16(uint8_t device_addr, uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;					// INA228 register address
    data[1] = (value >> 8) & 0xFF;  // MSB first
    data[2] = value & 0xFF;			// LSB
    return HAL_I2C_Master_Transmit(&hi2c1, device_addr, data, 3, INA228_I2C_TIMEOUT);
}

/* Helper function to read 16-bit register */
static HAL_StatusTypeDef INA228_ReadRegister16(uint8_t device_addr, uint8_t reg, uint16_t* value) {
    if (value == NULL) return HAL_ERROR;

    uint8_t data[2];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(&hi2c1, device_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 2, INA228_I2C_TIMEOUT);
    if (status == HAL_OK) {
    	// Reconstruct 16-bit value
        *value = ((uint16_t)data[0] << 8) | data[1];
    }
    return status;
}

/* Helper function to read 24-bit register (VSHUNT, VBUS, CURRENT) */
static HAL_StatusTypeDef INA228_ReadRegister24_20bit(uint8_t device_addr, uint8_t reg, int32_t* value) {
    if (value == NULL) return HAL_ERROR;

    uint8_t data[3];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(&hi2c1, device_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 3, INA228_I2C_TIMEOUT);
    if (status == HAL_OK) {
        // Reconstruct 24-bit value
        *value = ((int32_t)data[0] << 16) | ((int32_t)data[1] << 8) | data[2];

        // INA228 voltage/current registers use bits [23:4] for 20-bit data, bits [3:0] are reserved (always read 0)
        // Shift right by 4 to get the actual 20-bit data
        *value >>= 4;

        // Current and shunt registers are signed using 2's complement
        if (*value & 0x00080000) {  // Check bit 19 (sign bit of 20-bit value where 1=negative, 0=positive)
            *value |= 0xFFF00000;   // Extend from 20-bit to 32-bit by filling upper bits with 1s (indicate negative number)
        }
    }
    return status;
}

/* Helper function to read 24-bit register (POWER) */
static HAL_StatusTypeDef INA228_ReadRegister24_Full(uint8_t device_addr, uint8_t reg, uint32_t* value) {
    if (value == NULL) return HAL_ERROR;

    uint8_t data[3];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(&hi2c1, device_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 3, INA228_I2C_TIMEOUT);
    if (status == HAL_OK) {
        // Reconstruct full 24-bit value (POWER register uses all 24 bits/ has no reserved bits)
        *value = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    }
    return status;
}

/* Calculate calibration value */
static uint16_t INA228_CalculateCalibration(float shunt_resistor, float current_lsb) {
    // From Datasheet: SHUNT_CAL = 13107.2 × 10^6 × CURRENT_LSB × RSHUNT
    float cal = 13107200000.0f * current_lsb * shunt_resistor;
    return (uint16_t)(cal);
}

/* Initialize INA228 sensor */
HAL_StatusTypeDef INA228_Init(uint8_t device_addr, float shunt_resistor) {
    HAL_StatusTypeDef status;
    uint16_t config_value;
    uint16_t adc_config_value;
    uint16_t cal_value;

    // Reset device
    status = INA228_WriteRegister16(device_addr, INA228_REG_CONFIG, INA228_CONFIG_RST);
    if (status != HAL_OK) return status;

    HAL_Delay(10);  // Wait for reset

    // Configure device
    config_value = INA228_CONFIG_ADCRANGE | INA228_CONFIG_CONVDLY_0;
    status = INA228_WriteRegister16(device_addr, INA228_REG_CONFIG, config_value);
    if (status != HAL_OK) return status;

    // Configure ADC
    adc_config_value = INA228_ADC_MODE_CONT_ALL | INA228_ADC_VBUSCT_1052us | INA228_ADC_VSHCT_1052us | INA228_ADC_VTCT_1052us | INA228_ADC_AVG_64;
    status = INA228_WriteRegister16(device_addr, INA228_REG_ADC_CONFIG, adc_config_value);
    if (status != HAL_OK) return status;

    // Set calibration
    cal_value = INA228_CalculateCalibration(shunt_resistor, INA228_CURRENT_LSB);
    status = INA228_WriteRegister16(device_addr, INA228_REG_SHUNT_CAL, cal_value);

    return status;
}

/* Read bus voltage */
HAL_StatusTypeDef INA228_ReadVoltage(uint8_t device_addr, float* voltage) {
    if (voltage == NULL) return HAL_ERROR;

    int32_t raw_voltage;
    HAL_StatusTypeDef status;

    status = INA228_ReadRegister24_20bit(device_addr, INA228_REG_VBUS, &raw_voltage);
    if (status == HAL_OK) {
        *voltage = (float)raw_voltage * INA228_VBUS_LSB;  // Convert to volts
    }
    return status;
}

/* Read current */
HAL_StatusTypeDef INA228_ReadCurrent(uint8_t device_addr, float* current) {
    if (current == NULL) return HAL_ERROR;

    int32_t raw_current;
    HAL_StatusTypeDef status;

    status = INA228_ReadRegister24_20bit(device_addr, INA228_REG_CURRENT, &raw_current);
    if (status == HAL_OK) {
        *current = (float)raw_current * INA228_CURRENT_LSB;  // Convert to amps
    }
    return status;
}

/* Read power */
HAL_StatusTypeDef INA228_ReadPower(uint8_t device_addr, float* power) {
    if (power == NULL) return HAL_ERROR;

    uint32_t raw_power;
    HAL_StatusTypeDef status;

    status = INA228_ReadRegister24_Full(device_addr, INA228_REG_POWER, &raw_power);
    if (status == HAL_OK) {
        *power = (float)raw_power * INA228_POWER_LSB;  // Convert to watts
    }
    return status;
}

/* Check sensor health */
HAL_StatusTypeDef INA228_CheckHealth(uint8_t device_addr, uint8_t* healthy) {
	if (healthy == NULL) return HAL_ERROR;

    uint16_t diag_alert;
    HAL_StatusTypeDef status;

    status = INA228_ReadRegister16(device_addr, INA228_REG_DIAG_ALRT, &diag_alert);

    // Check MEMSTAT bit (bit 0) - should always be 1 for normal operation
	if (status == HAL_OK && (diag_alert & 0x0001)) {
		*healthy = 1;
	} else {
		*healthy = 0;
	}

    return status;
}

/* Configure alert thresholds */
HAL_StatusTypeDef INA228_ConfigureAlerts(uint8_t device_addr, float overvoltage_limit, float undervoltage_limit, float overcurrent_limit) {
    HAL_StatusTypeDef status;

    // Configure bus overvoltage limit (BOVL register)
    // Conversion: voltage (V) / LSB (V/bit) = register value
    uint16_t bovl_value = (uint16_t)(overvoltage_limit / INA228_BOVL_LSB);
    status = INA228_WriteRegister16(device_addr, INA228_REG_BOVL, bovl_value);
    if (status != HAL_OK) return status;

    // Configure bus undervoltage limit (BUVL register)
    uint16_t buvl_value = (uint16_t)(undervoltage_limit / INA228_BUVL_LSB);
    status = INA228_WriteRegister16(device_addr, INA228_REG_BUVL, buvl_value);
    if (status != HAL_OK) return status;

    // Configure shunt overvoltage limit (SOVL register) - overcurrent protection
    // Convert current limit to shunt voltage: V = I × R
    float shunt_voltage_limit = overcurrent_limit * SHUNT_RESISTOR;
    uint16_t sovl_value = (uint16_t)(shunt_voltage_limit / INA228_SOVL_LSB);
    status = INA228_WriteRegister16(device_addr, INA228_REG_SOVL, sovl_value);

    return status;
}


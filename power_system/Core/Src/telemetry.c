/*
 * telemetry.c
 *
 * CAN telemetry module for the exoskeleton power architecture system.
 * On each tick, reads the latest voltage and current from all enabled
 * INA228 sensors, pushes values through per-sensor circular buffers to
 * compute rolling averages, then packs the results into 7-byte CAN frames
 * (IDs 0x100–0x104) and transmits them on CAN1. 
 */

#include "telemetry.h"
#include "can.h"
#include <string.h>

// Circular Buffers for all 5 Sensors
// Index corresponds to INA228_Location_t: BUS=0, MOTOR1=1 ... MOTOR4=4
CircularBuffer_t g_voltage_buf[NUM_SENSORS];
CircularBuffer_t g_current_buf[NUM_SENSORS];

// Sensor configuration table
typedef struct {
    INA228_Location_t location;
    uint16_t          can_id;
    uint8_t           enabled;
} SensorConfig_t;

static SensorConfig_t sensors[NUM_SENSORS] = {
    { INA228_BUS,    CAN_ID_BUS,    0 },
    { INA228_MOTOR1, CAN_ID_MOTOR1, 0 },
    { INA228_MOTOR2, CAN_ID_MOTOR2, 0 },
    { INA228_MOTOR3, CAN_ID_MOTOR3, 0 },
    { INA228_MOTOR4, CAN_ID_MOTOR4, 0 },
};

// Enabled sensors for CAN channel
static const uint8_t enabled[NUM_SENSORS] = SENSOR_ENABLED;

/**
 * @brief CAN: Send one sensor frame
 *
 * Frame layout (DLC = 7):
 *   Byte 0-1 : voltage * 100  (int16, little-endian) -> divide by 100 on receiver
 *   Byte 2-3 : current * 100  (int16, little-endian) -> divide by 100 on receiver
 *   Byte 4	  : relay/contactor status (1 = closed)
 *   Byte 5   : sensor status		   (1 = healthy)
 *   Byte 6   : system fault		   (1 = fault active, bus frame only)
 *
 */
static void CAN_Send_INA228_Frame(uint16_t can_id, float voltage, float current, uint8_t closed, uint8_t sensor_status, uint8_t fault)
{
	CAN_TxHeaderTypeDef TxHeader;
	uint8_t  TxData[8] = {0};
	uint32_t TxMailbox;

    TxHeader.StdId = can_id;
    TxHeader.IDE   = CAN_ID_STD;
    TxHeader.RTR   = CAN_RTR_DATA;
    TxHeader.DLC   = 7;

    // Scale floats by 100 to preserve 2 decimal places as integers
    int16_t v = (int16_t)(voltage * 100.0f);
    int16_t c = (int16_t)(current * 100.0f);

    TxData[0] = v & 0xFF;			// Voltage LSB
    TxData[1] = (v >> 8) & 0xFF;	// Voltage MSB
    TxData[2] = c & 0xFF;			// Current LSB
    TxData[3] = (c >> 8) & 0xFF;	// Current MSB
    TxData[4] = closed;				// Relay or contactor status
    TxData[5] = sensor_status;		// Sensor status
	TxData[6] = fault;				// System fault

	// Send CAN frame if there's a free mailbox
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        HAL_StatusTypeDef ret = HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
        if (ret != HAL_OK){
        	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); // Blink if not sending data
        }

    }
}


// Public API Functions

/*
 * @brief Initialize all enabled sensors
 */
void telemetry_init(void)
{
	for(int i = 0; i < NUM_SENSORS; i++){
		circ_buf_init(&g_voltage_buf[i]);
		circ_buf_init(&g_current_buf[i]);
		sensors[i].enabled = enabled[i]; // Disabled sensors will be set to 0
	}
}

void telemetry_tick(void)
{
	uint8_t closed = (get_current_state() == STATE_NORMAL_OPERATION);  // Relay or contactor closed
    uint8_t fault = get_current_fault(); 		  					   // System fault

    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); // Debugging

    // Send CAN frames for all sensors
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {

        if (!sensors[i].enabled) continue; // Skip disabled sensors

        // Obtain latest sensor reading
        SensorData_t raw;
        get_sensor_data(sensors[i].location, &raw);

        // Push sensor data into rolling averages
        circ_buf_push(&g_voltage_buf[i], raw.voltage);
        circ_buf_push(&g_current_buf[i], raw.current);

        // Obtain average values
        float v_avg = circ_buf_average(&g_voltage_buf[i]);
        float c_avg = circ_buf_average(&g_current_buf[i]);

        // Send CAN frame of 1 sensor
        CAN_Send_INA228_Frame(sensors[i].can_id, v_avg, c_avg, closed, raw.healthy, fault);
        HAL_Delay(1);
    }
}



/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - precharge FSM + UART telemetry + CAN telemetry
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes */
#include "main.h"
#include "can.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"
#include "precharge.h"
#include "ina228_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* CAN Communication */
CAN_TxHeaderTypeDef TxHeader;
uint8_t  TxData[8];
uint32_t TxMailbox;
uint32_t last_can_tx_time = 0;
int can_tx_interval_ms = 100; // Send CAN frames every 100ms

/* UART Communication */
char rx_buf[RX_BUF_SIZE];
int  rx_index;

// Sampling Parameters
int sampling_rate = 0;   // Hz
int total_time = 0;   // seconds
int num_samples = 0;

// Data buffers for bus sensor
float voltage_buf[MAX_SAMPLES];
float current_buf[MAX_SAMPLES];
float power_buf[MAX_SAMPLES];

/* Private function prototypes */
void SystemClock_Config(void);

// CAN
static void CAN_Send_INA228_Data(INA228_Location_t location, uint16_t std_id);
static void CAN_Send_All_Sensors(void);

// UART
static void UART_ReceiveLine(void);
static int  Parse_Command(void);
static void Acquire_Data(void);
static void Transmit_Data(void);


/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();

  HAL_CAN_Start(&hcan1);

  // Initialize precharge FSM (also initializes all 5 INA228 sensors internally)
  precharge_control_init();

  /* Main Loop */
  while (1)
  {
      // Precharge FSM tick (sensors internally polled on SENSOR_POLL_INTERVAL_MS)
      precharge_fsm_tick();

      // CAN telemetry (non-blocking)
      uint32_t now = HAL_GetTick();
      if (now - last_can_tx_time >= can_tx_interval_ms) {
          CAN_Send_All_Sensors();
          last_can_tx_time = now;
      }

      // UART telemetry (for Python data logging plots)
      if (get_current_state() == STATE_NORMAL_OPERATION) {

          UART_ReceiveLine();

          if (Parse_Command()) {
              const char ok[] = "OK\n"; // response for Python script to check
              HAL_UART_Transmit(&huart2, (uint8_t *)ok, sizeof(ok) - 1, HAL_MAX_DELAY);
              Acquire_Data();
              Transmit_Data();
          }
          else {
			  const char err[] = "ERR\n"; // response for Python script to check
			  HAL_UART_Transmit(&huart2, (uint8_t *)err, sizeof(err) - 1, HAL_MAX_DELAY);
          }
      }
  }
}

/**
 * @brief CAN: Send one sensor frame
 *
 * Frame layout (DLC = 8):
 *   Byte 0-1 : voltage * 100  (int16, little-endian) -> divide by 100 on receiver
 *   Byte 2-3 : current * 100  (int16, little-endian) -> divide by 100 on receiver
 *   Byte 4-5 : power   * 100  (int16, little-endian) -> divide by 100 on receiver
 *   Byte 6   : healthy        (0 = fault, 1 = ok)
 *   Byte 7   : fault code     (FaultType_t, 0 = FAULT_NONE)
 *
 */
static void CAN_Send_INA228_Data(INA228_Location_t location, uint16_t std_id)
{
    SensorData_t sensor;
    get_sensor_data(location, &sensor);

    TxHeader.StdId = std_id;
    TxHeader.IDE   = CAN_ID_STD;
    TxHeader.RTR   = CAN_RTR_DATA;
    TxHeader.DLC   = 8;

    // Scale floats by 100 to preserve 2 decimal places as integers
    int16_t voltage = (int16_t)(sensor.voltage * 100.0f);
    int16_t current = (int16_t)(sensor.current * 100.0f);
    int16_t power   = (int16_t)(sensor.power   * 100.0f);
    uint8_t healthy = sensor.healthy;
    uint8_t fault   = (uint8_t)get_current_fault();  // System-wide fault code

    // Pack Voltage
    TxData[0] = voltage & 0xFF;
    TxData[1] = (voltage >> 8) & 0xFF;

    // Pack Current
    TxData[2] = current & 0xFF;
    TxData[3] = (current >> 8) & 0xFF;

    // Pack Power
    TxData[4] = power & 0xFF;
    TxData[5] = (power >> 8) & 0xFF;

    // Pack Health + Fault
    TxData[6] = healthy;
    TxData[7] = fault;

    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0) {
        HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
    }
}

/**
  * @brief CAN: Broadcast all 5 sensor frames
  *
  * Each INA228 sensor has a unique CAN ID defined in precharge.h
  */
static void CAN_Send_All_Sensors(void)
{
    CAN_Send_INA228_Data(INA228_MOTOR1, CAN_ID_MOTOR1);
    CAN_Send_INA228_Data(INA228_MOTOR2, CAN_ID_MOTOR2);
    CAN_Send_INA228_Data(INA228_MOTOR3, CAN_ID_MOTOR3);
    CAN_Send_INA228_Data(INA228_MOTOR4, CAN_ID_MOTOR4);
    CAN_Send_INA228_Data(INA228_BUS, CAN_ID_BUS);

    // Blink LED once per full broadcast cycle to verify STM32 is successfully queuing frames into the CAN mailbox
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}

/**
  * @brief UART: Receive one line terminated by '\n'
  */
static void UART_ReceiveLine(void)
{
    rx_index = 0;
    memset(rx_buf, 0, sizeof(rx_buf));

    while (1)
    {
        uint8_t byte;
        HAL_UART_Receive(&huart2, &byte, 1, HAL_MAX_DELAY);

        if (byte == '\n') break;

        if (rx_index < RX_BUF_SIZE - 1)
            rx_buf[rx_index++] = (char)byte;
    }
}

/**
  * @brief UART: Parse command "START,<rate_hz>,<time_s>"
  * @retval 1 on valid command, 0 on error
  */
static int Parse_Command(void)
{
    if (strncmp(rx_buf, "START", 5) != 0) return 0;

    char *tok = strtok(rx_buf, ",");   // consume "START"
    tok = strtok(NULL, ",");
    if (!tok) return 0;
    sampling_rate = atoi(tok);

    tok = strtok(NULL, ",");
    if (!tok) return 0;
    total_time = atoi(tok);

    if (sampling_rate <= 0 || total_time <= 0) return 0;

    num_samples = sampling_rate * total_time;
    if (num_samples > MAX_SAMPLES)
        num_samples = MAX_SAMPLES;

    return 1;
}

/**
  * @brief UART: Acquire data from bus sensor
  *
  * To log a different sensor, change INA228_BUS + BUS_CURRENT_LSB + BUS_POWER_LSB to the desired location and LSB constants
  */
static void Acquire_Data(void)
{
    uint8_t healthy;
    INA228_CheckHealth(INA228_ADDR5, &healthy);

    if (!healthy)
    {
        for (int i = 0; i < num_samples; i++)
            voltage_buf[i] = current_buf[i] = power_buf[i] = 0.0f;
        return;
    }

    uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;

    for (int i = 0; i < num_samples; i++)
    {
        INA228_ReadVoltage(INA228_ADDR5, &voltage_buf[i]);
        INA228_ReadCurrent(INA228_ADDR5, &current_buf[i], BUS_CURRENT_LSB);
        INA228_ReadPower(INA228_ADDR5, &power_buf[i], BUS_POWER_LSB);
        HAL_Delay(delay_ms);
    }
}

/**
  * @brief UART: Transmit logged data to PC
  *
  * Format per line: voltage,current,power\n
  * Terminated with "DONE\n"
  */
static void Transmit_Data(void)
{
    char line[64];

    for (int i = 0; i < num_samples; i++)
    {
        int len = snprintf(line, sizeof(line),
                           "%.4f,%.4f,%.4f\n",
                           voltage_buf[i],
                           current_buf[i],
                           power_buf[i]);
        HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
    }

    const char done[] = "DONE\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)done, sizeof(done) - 1, HAL_MAX_DELAY);
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

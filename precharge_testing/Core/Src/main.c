/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

#include "precharge.h"
#include "ina228_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* UART Communication --------------------------------------------------------*/
#define RX_BUF_SIZE  64
#define MAX_SAMPLES  2000

volatile uint8_t uart_rx_byte;
volatile uint8_t uart_line_ready = 0;

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

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

// UART
static int  Parse_Command(void);
static void Acquire_Data(void);
static void Transmit_Data(void);

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();


  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();

  // Arm interrupt-driven UART (MCU receives bytes in the background without blocking so that the FSM can run freely)
  HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);

  // Initialize precharge FSM (also initializes all INA228 sensors internally)
  precharge_control_init();
  const char msg[] = "INIT\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, sizeof(msg) - 1, HAL_MAX_DELAY); // Confirm initialization

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	// Precharge FSM tick (sensors internally polled on SENSOR_POLL_INTERVAL_MS)
	precharge_fsm_tick();

	// Check for UART interrupt
	if (uart_line_ready) {
		uart_line_ready = 0;
		if (Parse_Command()) {
			const char ok[] = "OK\n";
			HAL_UART_Transmit(&huart2, (uint8_t *)ok, sizeof(ok) - 1, HAL_MAX_DELAY);
			Acquire_Data();
			Transmit_Data();
		} else {
			const char err[] = "ERR\n";
			HAL_UART_Transmit(&huart2, (uint8_t *)err, sizeof(err) - 1, HAL_MAX_DELAY);
		}
	}

  }
  /* USER CODE END 3 */
}


// Called automatically by HAL every time one byte arrives
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (uart_rx_byte == '\n') {
            rx_buf[rx_index] = '\0';
            uart_line_ready = 1;   // signal to main loop
            rx_index = 0;
        } else if (rx_index < RX_BUF_SIZE - 1) {
            rx_buf[rx_index++] = (char)uart_rx_byte;
        }
        // Re-arm for next byte
        HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
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
	SensorData_t sensor;
	get_sensor_data(INA228_BUS, &sensor);

    if (!sensor.healthy)
    {
        FaultType_t fault = get_current_fault();
        const char *fault_msg = NULL;

        switch (fault) {
            case FAULT_BUS_OVERCURRENT:   fault_msg = "FAULT_BUS_OVERCURRENT\n";   break;
            case FAULT_BUS_OVERVOLTAGE:   fault_msg = "FAULT_BUS_OVERVOLTAGE\n";   break;
            case FAULT_BUS_UNDERVOLTAGE:  fault_msg = "FAULT_BUS_UNDERVOLTAGE\n";  break;
            case FAULT_SENSOR_COMM:       fault_msg = "FAULT_SENSOR_COMM\n";       break;
            case FAULT_MOTOR_OVERCURRENT: fault_msg = "FAULT_MOTOR_OVERCURRENT\n"; break;
            default:                      fault_msg = "FAULT_UNKNOWN\n";           break;
        }

        if (fault_msg)
            HAL_UART_Transmit(&huart2, (uint8_t *)fault_msg, strlen(fault_msg), HAL_MAX_DELAY);

        for (int i = 0; i < num_samples; i++)
            voltage_buf[i] = current_buf[i] = power_buf[i] = 0.0f;
        return;
    }

    uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;

    for (int i = 0; i < num_samples; i++)
    {
    	precharge_fsm_tick();
    	get_sensor_data(INA228_BUS, &sensor);
        voltage_buf[i] = sensor.voltage;
        current_buf[i] = sensor.current;
        power_buf[i]   = sensor.power;
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

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
#include "can.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"
#include "ina228_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "can/can_app.h"
#include "can/can_bus_stm32.hpp"


/* ================= USER DEFINES ================= */
#define RX_BUF_SIZE    64
#define MAX_SAMPLES    10000
/* =============================================== */


/* Sampling parameters */
static int sampling_rate = 128;   // Hz
static int total_time    = 10;   // seconds

/* Data buffers */
static float voltage_buf[MAX_SAMPLES];
static float current_buf[MAX_SAMPLES];
static float power_buf[MAX_SAMPLES];

static uint16_t power_id=0x123;			//CAN ID for power MCU
static uint16_t dashboard_id=0x124;		//CAN ID for dashboard MCU



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void Acquire_Data(void);
void Data_Average(float* voltage, float* current, float* power);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  CanApp_Init();
  MX_CAN1_Init();
  MX_CAN2_Init();
  if (INA228_Init(INA228_ADDR, 0.01f) != HAL_OK){
	  //if we want to do anything if initialization fails, we do it here
	  ;
  }

  //This is the main loop. The idea is that we want to acquire data, then transmit it when dashboard
  //requests it.
  //This program will collect a five point average for voltage and current average
  float v_avg=0;
  float i_avg=0;
  float p_avg=0;

  //write code in while loop to collect average then check if dashboard is requesting it.

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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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

static void Acquire_Data(void)
{
  uint8_t healthy = 0;
  INA228_CheckHealth(INA228_ADDR, &healthy);
  int num_samples=sampling_rate*total_time;
  if (!healthy)
  {
    for (int i = 0; i < num_samples; i++)
      voltage_buf[i] = current_buf[i] = power_buf[i] = 0.0f;
    return;
  }

  uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;

  for (int i = 0; i < num_samples; i++)
  {
    float v = 0.0f, c = 0.0f, p = 0.0f;

    INA228_ReadVoltage(INA228_ADDR, &v);
    INA228_ReadCurrent(INA228_ADDR, &c);
    INA228_ReadPower  (INA228_ADDR, &p);

    voltage_buf[i] = v;
    current_buf[i] = c;
    power_buf[i]   = p;

    HAL_Delay(delay_ms);
  }
}

void Data_Average(float *voltage, float *current, float *power)
{
	uint8_t healthy = 0;
	INA228_CheckHealth(INA228_ADDR, &healthy);
	int num_samples=sampling_rate*total_time;
	if (!healthy)
	{
		INA228_ReadVoltage(INA228_ADDR, voltage);
		INA228_ReadCurrent(INA228_ADDR, current);
		INA228_ReadPower  (INA228_ADDR, power);
	}
	uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;
	HAL_Delay(delay_ms);

}


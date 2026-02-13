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
#include "can_bus.h"


/*--------------------------------------------------------------------------*/
/*------------------------------DASHBOARD PARAMETERS------------------------*/
/*--------------------------------------------------------------------------*/
/*CHANGE NUMBER OF SAMPLES IN FLOATING POINT AVERAGE HERE*/
#define FLOAT_AVG_SIZE   5
static int sampling_rate = 10;   // Hz
#define DASHBOARD_COMMAND   1000	//Arbitrary message to send data


#define RX_BUF_SIZE    64
static char rxBuf[RX_BUF_SIZE];
static int  rxIndex;


/* AVERAGE BUFFERS */
static float voltage_avg1[FLOAT_AVG_SIZE];
static float current_avg1[FLOAT_AVG_SIZE];
static float power_avg1[FLOAT_AVG_SIZE];

static float voltage_avg2[FLOAT_AVG_SIZE];
static float current_avg2[FLOAT_AVG_SIZE];
static float power_avg2[FLOAT_AVG_SIZE];

static float ERROR_VALUE=100.0f;



/*--------------------------------------------------------------------------*/
/*----------------------------LAPTOP UART PARAMETERS------------------------*/
/*--------------------------------------------------------------------------*/
#define MAX_SAMPLES    2000
static int total_time    = 0;   // seconds
static int num_samples   = 0;
static float voltage_buf1[MAX_SAMPLES];
static float current_buf1[MAX_SAMPLES];
static float power_buf1[MAX_SAMPLES];

static float voltage_buf2[MAX_SAMPLES];
static float current_buf2[MAX_SAMPLES];
static float power_buf2[MAX_SAMPLES];


void SystemClock_Config(void);

/*--------------------------------------------------------------------------*/
/*-------------------------DASHBOARD RELATED FUNCTIONS----------------------*/
/*--------------------------------------------------------------------------*/
static void Floating_Average(int i);
static void Dashboard_Transmit_UART(void);
int Dashboard_Receive_UART(void);
static void Dashboard_Transmit_CAN(void);
int Dashboard_Receive_CAN(void);
/*--------------------------------------------------------------------------*/
/*----------------------------===LAPTOP UART FUNCTIONS----------------------*/
/*--------------------------------------------------------------------------*/
static void Acquire_Data(void);
static void UART_ReceiveLine(void);
static void Transmit_Data(void);


int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  //MX_CAN1_Init();
  //MX_CAN2_Init();
  MX_USART1_UART_Init();
  can_bus_init(&hcan1);

  HAL_GPIO_WritePin(GPIOB,
                    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2,
                    GPIO_PIN_RESET);


  if (INA228_Init(INA228_ADDR1, 0.015f) != HAL_OK)
  {
    const char *msg = "INA228 #1 INIT FAILED\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
  }
  if (INA228_Init(INA228_ADDR2, 0.015f) != HAL_OK)
  {
    const char *msg = "INA228 #2 INIT FAILED\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
  }



  int sample_number = 0;
  uint32_t last_transmit_ms = 0;
  const uint32_t transmit_interval_ms = 100;  // 10Hz transmission rate

  while (1)
  {
    // Read and update floating average
    Floating_Average(sample_number);
    sample_number = (sample_number + 1) % FLOAT_AVG_SIZE;

    // Automatically transmit data at 10Hz for dashboard
    uint32_t now = HAL_GetTick();
    if (now - last_transmit_ms >= transmit_interval_ms)
    {
      last_transmit_ms = now;
      Dashboard_Transmit_UART();
    }
  }
}


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
  uint8_t healthy1 = 0, healthy2 = 0;
  INA228_CheckHealth(INA228_ADDR1, &healthy1);
  INA228_CheckHealth(INA228_ADDR2, &healthy2);
  /*Five sensors*/
  /*uint8_t healthy3 = 0, healthy4 = 0, healthy5=0;
  INA228_CheckHealth(INA228_ADDR1, &healthy3);
  INA228_CheckHealth(INA228_ADDR2, &healthy4);
  INA228_CheckHealth(INA228_ADDR2, &healthy5);*/

  if (!healthy1 || !healthy2/*|| !healthy3 || !healthy4 || !healthy5*/)
  {
    for (int i = 0; i < num_samples; i++) {
    	voltage_buf1[i] = current_buf1[i] = power_buf1[i] = 0.0f;
    	voltage_buf2[i] = current_buf2[i] = power_buf2[i] = 0.0f;
    	/*
    	voltage_buf3[i] = current_buf3[i] = power_buf3[i] = 0.0f;
    	voltage_buf4[i] = current_buf4[i] = power_buf4[i] = 0.0f;
    	voltage_buf5[i] = current_buf5[i] = power_buf5[i] = 0.0f;
    	*/

    }
    return;
  }

  uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;

  for (int i = 0; i < num_samples; i++)
  {
    float v1 = 0.0f, c1 = 0.0f, p1 = 0.0f;
    float v2 = 0.0f, c2 = 0.0f, p2 = 0.0f;
    /*
    float v3 = 0.0f, c3 = 0.0f, p3 = 0.0f;
    float v4 = 0.0f, c4 = 0.0f, p4 = 0.0f;
    float v5 = 0.0f, c5 = 0.0f, p5 = 0.0f;
    */

    INA228_ReadVoltage(INA228_ADDR1, &v1);
    INA228_ReadCurrent(INA228_ADDR1, &c1);
    INA228_ReadPower  (INA228_ADDR1, &p1);

    INA228_ReadVoltage(INA228_ADDR2, &v2);
    INA228_ReadCurrent(INA228_ADDR2, &c2);
    INA228_ReadPower  (INA228_ADDR2, &p2);

    /*
    INA228_ReadVoltage(INA228_ADDR3, &v3);
    INA228_ReadCurrent(INA228_ADDR3, &c3);
    INA228_ReadPower  (INA228_ADDR3, &p3);

    INA228_ReadVoltage(INA228_ADDR4, &v4);
    INA228_ReadCurrent(INA228_ADDR4, &c4);
    INA228_ReadPower  (INA228_ADDR4, &p4);

    INA228_ReadVoltage(INA228_ADDR5, &v5);
    INA228_ReadCurrent(INA228_ADDR5, &c5);
    INA228_ReadPower  (INA228_ADDR5, &p5);
    */

    voltage_buf1[i] = v1;
    current_buf1[i] = c1;
    power_buf1[i]   = p1;

    voltage_buf2[i] = v2;
    current_buf2[i] = c2;
    power_buf2[i]   = p2;

    /*
    voltage_buf2[i] = v3;
    current_buf2[i] = c3;
    power_buf2[i]   = p3;

    voltage_buf2[i] = v4;
    current_buf2[i] = c4;
    power_buf2[i]   = p4;

    voltage_buf2[i] = v5;
    current_buf2[i] = c5;
    power_buf2[i]   = p5;
     */

    HAL_Delay(delay_ms);
  }
}
static void Floating_Average(int i)
{
	uint8_t healthy1 = 0, healthy2 = 0;
	INA228_CheckHealth(INA228_ADDR1, &healthy1);
	INA228_CheckHealth(INA228_ADDR2, &healthy2);

	/*Five sensors
  	  uint8_t healthy3 = 0, healthy4 = 0, healthy5=0;
  	  INA228_CheckHealth(INA228_ADDR1, &healthy3);
  	  INA228_CheckHealth(INA228_ADDR2, &healthy4);
  	  INA228_CheckHealth(INA228_ADDR2, &healthy5);
	 */

	if (!healthy1)
	{
		for (int y=0; y<FLOAT_AVG_SIZE;y++)
		{
		  voltage_avg1[y] = current_avg1[y] = power_avg1[y] = ERROR_VALUE;
		}

	 }

	if (!healthy2)
    {
  	  for (int y=0; y<FLOAT_AVG_SIZE;y++)
  		{
  		  voltage_avg2[y] = current_avg2[y] = power_avg2[y] = ERROR_VALUE;
  		}

    }
	/*
	if (!healthy3)
    {
  	  for (int y=0; y<FLOAT_AVG_SIZE;y++)
  		{
  		  voltage_avg3[y] = current_avg3[y] = power_avg3[y] = ERROR_VALUE;
  		}
  	}
if (!healthy4)
    {
  	  for (int y=0; y<FLOAT_AVG_SIZE;y++)
  		{
  		  voltage_avg4[y] = current_avg4[y] = power_avg4[y] = ERROR_VALUE;
  		}

    }

  if (!healthy5)
    {
  	  for (int y=0; y<FLOAT_AVG_SIZE;y++)
  		{
  		  voltage_avg5[y] = current_avg5[y] = power_avg5[y] = ERROR_VALUE;
  		}

    }
    */

	uint32_t delay_ms = (sampling_rate > 0) ? (1000 / sampling_rate) : 1;
	INA228_ReadVoltage(INA228_ADDR1, &voltage_avg1[i]);
	INA228_ReadCurrent(INA228_ADDR1, &current_avg1[i]);
	INA228_ReadPower  (INA228_ADDR1, &power_avg1[i]);

	INA228_ReadVoltage(INA228_ADDR2, &voltage_avg2[i]);
	INA228_ReadCurrent(INA228_ADDR2, &current_avg2[i]);
	INA228_ReadPower  (INA228_ADDR2, &power_avg2[i]);

	/*
	INA228_ReadVoltage(INA228_ADDR3, &voltage_avg3[i]);
	INA228_ReadCurrent(INA228_ADDR3, &current_avg3[i]);
	INA228_ReadPower  (INA228_ADDR3, &power_avg3[i]);

	INA228_ReadVoltage(INA228_ADDR4, &voltage_avg4[i]);
	INA228_ReadCurrent(INA228_ADDR4, &current_avg4[i]);
	INA228_ReadPower  (INA228_ADDR4, &power_avg4[i]);

	INA228_ReadVoltage(INA228_ADDR5, &voltage_avg5[i]);
	INA228_ReadCurrent(INA228_ADDR5, &current_avg5[i]);
	INA228_ReadPower  (INA228_ADDR5, &power_avg5[i]);
	 */

    HAL_Delay(delay_ms);
}


static void Dashboard_Transmit_UART(void)
{
  char line[256];
  float V1_avg = 0;
  float I1_avg = 0;
  float P1_avg = 0;
  float V2_avg = 0;
  float I2_avg = 0;
  float P2_avg = 0;

  uint8_t healthy1 = 0, healthy2 = 0;
  INA228_CheckHealth(INA228_ADDR1, &healthy1);
  INA228_CheckHealth(INA228_ADDR2, &healthy2);

  for (int i = 0; i < FLOAT_AVG_SIZE; i++)
  {
    V1_avg += voltage_avg1[i];
    I1_avg += current_avg1[i];
    P1_avg += power_avg1[i];
    V2_avg += voltage_avg2[i];
    I2_avg += current_avg2[i];
    P2_avg += power_avg2[i];
  }

  V1_avg /= FLOAT_AVG_SIZE;
  I1_avg /= FLOAT_AVG_SIZE;
  P1_avg /= FLOAT_AVG_SIZE;
  V2_avg /= FLOAT_AVG_SIZE;
  I2_avg /= FLOAT_AVG_SIZE;
  P2_avg /= FLOAT_AVG_SIZE;

  int len = snprintf(line, sizeof(line),
                     "{\"voltage\":%.3f,\"current\":%.3f,\"power\":%.3f,"
                     "\"healthy\":%d,"
                     "\"voltage2\":%.3f,\"current2\":%.3f,\"power2\":%.3f,"
                     "\"healthy2\":%d}\n",
                     V1_avg, I1_avg, P1_avg, healthy1,
                     V2_avg, I2_avg, P2_avg, healthy2);

  HAL_UART_Transmit(&huart1, (uint8_t*)line, len, HAL_MAX_DELAY);
}

int Dashboard_Receive_UART(void)
{
  rxIndex = 0;
  memset(rxBuf, 0, sizeof(rxBuf));
  uint8_t byte;
  HAL_UART_Receive(&huart1, &byte, 1, HAL_MAX_DELAY);
  if (byte == DASHBOARD_COMMAND)
  {
	  return 1;
  }
  return 0;
}

static void UART_ReceiveLine(void)
{
  rxIndex = 0;
  memset(rxBuf, 0, sizeof(rxBuf));

  while (1)
  {
    uint8_t byte;
    HAL_UART_Receive(&huart2, &byte, 1, HAL_MAX_DELAY);

    if (byte == '\n')
      break;

    if (rxIndex < RX_BUF_SIZE - 1)
      rxBuf[rxIndex++] = (char)byte;
  }
}

static void Transmit_Data(void)
{
  char line[RX_BUF_SIZE];

  for (int i = 0; i < num_samples; i++)
  {
    int len = snprintf(line, sizeof(line),
                       "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                       voltage_buf1[i],
                       current_buf1[i],
                       power_buf1[i],
					   voltage_buf2[i],
                       current_buf2[i],
                       power_buf2[i]);

    HAL_UART_Transmit(&huart2, (uint8_t*)line, len, HAL_MAX_DELAY);
  }

  const char done[] = "DONE\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)done, sizeof(done)-1, HAL_MAX_DELAY);
}

static void Dashboard_Transmit_CAN(void)
{
	  char line[RX_BUF_SIZE];
	  float V1_avg=0;
	  float I1_avg=0;
	  float P1_avg=0;
	  float V2_avg=0;
	  float I2_avg=0;
	  float P2_avg=0;

	  for (int i=0; i<FLOAT_AVG_SIZE;i++)
	  {
		  V1_avg+=voltage_avg1[i];
		  I1_avg+=current_avg1[i];
		  P1_avg+=power_avg1[i];
		  V2_avg+=voltage_avg2[i];
		  I2_avg+=current_avg2[i];
		  P2_avg+=power_avg2[i];
	  }

	  V1_avg/=FLOAT_AVG_SIZE;
	  I1_avg/=FLOAT_AVG_SIZE;
	  P1_avg/=FLOAT_AVG_SIZE;
	  V2_avg/=FLOAT_AVG_SIZE;
	  I2_avg/=FLOAT_AVG_SIZE;
	  P2_avg/=FLOAT_AVG_SIZE;



}

int Dashboard_Receive_CAN(void)
{

}


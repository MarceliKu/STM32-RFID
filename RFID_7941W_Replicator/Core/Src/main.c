/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file       : main.c
  * @author     : MarceliKu
  * @brief      : RFID tag replicator (125kHz, 13.56MHz) with using 7941W module
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ring_buffer.h"
#include "parser.h"
#include "stdio.h"
#include "string.h"
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIMER_SEND_NEXT_REQUEST 700
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t Text[90];
uint8_t TextLen;
uint8_t RequetsSent = 0; // How many frames (requests via USART1) have already been sent?
uint8_t CurrentMode = 0;

// RFID communication (USART1)
RingBuffer_t ReceiveBuffer;
uint8_t      ReceiveTmp;
extern char stringBuffer[525];

// Software timers
uint32_t TimerMsgSend;   // Software timer for sending next message by UART

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_NVIC_Init(void);
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
  MX_USART1_UART_Init();
  MX_LPUART1_UART_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  PRS_Outcome ParseOutcome;

  //
  // Declarations and example(!) values of messages (requests) for 7941W
  //
  uint8_t MsgReadID[]     = { 0xAB, 0xBA, 0x00, 0x15, 0x00, 0x15};  // 0x15 requests reading ID of RFID tag of 125 kHz
  uint8_t MsgReadUID[]    = { 0xAB, 0xBA, 0x00, 0x10, 0x00, 0x10};  // 0x10 requests reading ID of RFID tag of 13.56 MHz
  uint8_t MsgWriteID[]    = { 0xAB, 0xBA, 0x00, 0x16, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05, 0x12};  // 0x16 requests writing new ID (5 bytes)
  uint8_t MsgWriteUID[]   = { 0xAB, 0xBA, 0x00, 0x11, 0x04, 0x01, 0x02, 0x03, 0x04, 0x11};  // 0x11 requests writing new Buffer (4 bytes)

  uint8_t OrgPass[] = {0X00, 0X00, 0X00, 0X00, 0X00};
  uint8_t* RequestPtr;
  uint8_t RequestLen;
  uint8_t XorByte;
  char    IdString[17];
  char    IdToVerify[17];
  uint8_t BeginOfIdLoc = 15;
  uint8_t IdLen = 14;
  uint8_t UIdLen = 11;
  uint8_t WhatWasReceived = 0;  // 0 - nothing, 1 - ID, 2 - UID
  uint8_t WhatToSend = 0;       // 0 - send request for ID, 1 - UID

  // Start copy procedure
  TextLen = sprintf((char *)Text, "\n\rWelcome to RFID tag replicator.\n\r");
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
  TextLen = sprintf((char *)Text, "==============================\n\r");
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
  TextLen = sprintf((char *)Text, "Step 1 - Put an RFID tag on the 7941W module\n\r");
  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
  CurrentMode = 1;

  // Start listening to UART
  HAL_UART_Receive_IT(&huart1, &ReceiveTmp, 1);

  // Initiate TimerMsgSend
  TimerMsgSend   = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  //=======================================================================================================
	  // Handle blue button
	  //=======================================================================================================

	  if(GPIO_PIN_SET == HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin))
	  {
		  HAL_Delay(100);

		  if(GPIO_PIN_SET == HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin))
		  {
			  TextLen = sprintf((char *)Text, "\n\rLet's start again.\n\r");
			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  TextLen = sprintf((char *)Text, "==================\n\r");
			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  TextLen = sprintf((char *)Text, "\n\rStep 1 - Put an RFID tag on the 7941W module\n\r");
			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  OrgPass[0] = 0;
			  OrgPass[1] = 0;
			  OrgPass[2] = 0;
			  OrgPass[3] = 0;
			  OrgPass[4] = 0;
			  CurrentMode = 1;
			  RequetsSent = 0;
			  WhatWasReceived = 0;
		  }
	  }


	  //=======================================================================================================
	  // Parse byte received from 7941W (UART1)
	  //=======================================================================================================
	  ParseOutcome = Parse_Byte(&ReceiveBuffer);
	  if(ParseOutcome == PRS_MSG_RECEIVED)
	  {
		  switch(CurrentMode)
		  {
		  case 1: // Step 1 - Read RFID Tag ID/UID to be replicated
			  // If it was positive (0x81) response get Id from it and send to "console" (LPUART1)
			  if(stringBuffer[9] == '8' && stringBuffer[10] == '1')
			  {
				  if(stringBuffer[12] == '0')
				  {
					  if(stringBuffer[13] == '5')
					  {
						  TextLen = sprintf((char *)Text, "Received ID: ");
						  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

						  strncpy(IdString, stringBuffer + BeginOfIdLoc, IdLen); // extract Id from stringBuffer
						  IdString[14] = '\n';
						  IdString[15] = '\r';
						  IdString[16] = '\0';
						  OrgPass[0] = hex2uint(IdString[0], IdString[1]);
						  OrgPass[1] = hex2uint(IdString[3], IdString[4]);
						  OrgPass[2] = hex2uint(IdString[6], IdString[7]);
						  OrgPass[3] = hex2uint(IdString[9], IdString[10]);
						  OrgPass[4] = hex2uint(IdString[12], IdString[13]);

						  HAL_UART_Transmit(&hlpuart1, (uint8_t *)IdString, strlen(IdString), 1000);
						  WhatWasReceived = 1;
						  TextLen = sprintf((char *)Text, "\n\rStep 2 - Put a target RFID tag (125 kHz) on the 7941W module\n\r");
						  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
						  CurrentMode = 2;
					  }
					  else if(stringBuffer[13] == '4')
					  {
						  TextLen = sprintf((char *)Text, "Received UID: ");
						  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);


						  strncpy(IdString, stringBuffer + BeginOfIdLoc, UIdLen); // extract Id from stringBuffer
						  IdString[11] = '\n';
						  IdString[12] = '\r';
						  IdString[13] = '\0';
						  OrgPass[0] = hex2uint(IdString[0], IdString[1]);
						  OrgPass[1] = hex2uint(IdString[3], IdString[4]);
						  OrgPass[2] = hex2uint(IdString[6], IdString[7]);
						  OrgPass[3] = hex2uint(IdString[9], IdString[10]);
						  HAL_UART_Transmit(&hlpuart1, (uint8_t *)IdString, strlen(IdString), 1000);
						  WhatWasReceived = 2; //(13.56 MHz)
						  TextLen = sprintf((char *)Text, "\n\rStep 2 - Put a target RFID tag (13.56 MHz) on the 7941W module\n\r");
						  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
						  CurrentMode = 2;
					  }
				  }
			  }
			  break;

		  case 2: // Step 2 - Wait for proper Tag
			  // If it was positive (0x81) response check if it is correct type (same as original)
			  if(stringBuffer[9] == '8' && stringBuffer[10] == '1')
			  {
				  if(stringBuffer[12] == '0')
				  {
					  if(stringBuffer[13] == '5' && WhatWasReceived == 1)
					  {
						  strncpy(IdToVerify, stringBuffer + BeginOfIdLoc, IdLen); // extract Id to verify from string
						  if(strncmp(IdString, IdToVerify, IdLen) != 0) // check if it is still the same Tag (the same ID/UID)
						  {
							  TextLen = sprintf((char *)Text, "\n\rStep 3 - Saving tag ID...\n\r");
							  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
							  CurrentMode = 3;
						  }


					  }
					  else if(stringBuffer[13] == '4' && WhatWasReceived == 2)
					  {
						  strncpy(IdToVerify, stringBuffer + BeginOfIdLoc, UIdLen); // extract Id to verify from string

						  if(strncmp(IdString, IdToVerify, UIdLen) != 0) // check if it is still the same Tag (the same ID/UID)
						  {
							  TextLen = sprintf((char *)Text, "\n\rStep 3 - Saving tag UID...\n\r");
							  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
							  CurrentMode = 3;
						  }
					  }
				  }
			  }
			  break;
		  case 3: // Step 3 - Write ID/UID on destination Tag
			  // If it was positive (0x81) response then replication done
			  if(stringBuffer[9] == '8' && stringBuffer[10] == '1')
			  {
				  TextLen = sprintf((char *)Text, "Done. Replication successful..\n\r");
				  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  }
			  // If the answer was negative (0x80), replication failed.
			  if(stringBuffer[9] == '8' && stringBuffer[10] == '0')
			  {
				  TextLen = sprintf((char *)Text, "Sorry - replication failed.\n\r");
				  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  }
			  TextLen = sprintf((char *)Text, "\n\rTo start again press blue button.\n\r");
			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
			  CurrentMode = 0;
			  break;
		  }
		  // get ready for new message
		  Reset_Buffers();
	  }

	  //=======================================================================================================
	  // Send next request
	  //=======================================================================================================

	  if((HAL_GetTick() - TimerMsgSend) > TIMER_SEND_NEXT_REQUEST)
	  {
		  TimerMsgSend = HAL_GetTick();
		  switch(CurrentMode)
		  {
		  case 1:
		  case 2:
			  if(WhatToSend == 0)
			  {
				  RequestPtr = MsgReadID;
				  RequestLen = sizeof MsgReadID;
				  HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
				  WhatToSend = 1;
			  }
			  else
			  {
				  RequestPtr = MsgReadUID;
				  RequestLen = sizeof MsgReadUID;
				  HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
				  WhatToSend = 0;
			  }
			  break;
		  case 3:
			  if(RequetsSent < 1) // to send this request only once
			  {
				  if(WhatWasReceived == 1)
				  {
					  RequestPtr = MsgWriteID;
					  RequestLen = sizeof MsgWriteID;
					  RequestPtr[5] = OrgPass[0];
					  RequestPtr[6] = OrgPass[1];
					  RequestPtr[7] = OrgPass[2];
					  RequestPtr[8] = OrgPass[3];
					  RequestPtr[9] = OrgPass[4];
					  XorByte = CalculateCheckByte(RequestPtr, RequestLen);
					  RequestPtr[RequestLen - 1] = XorByte;
					  RequetsSent++;
					  HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
				  }
				  else
				  {
					  RequestPtr = MsgWriteUID;
					  RequestLen = sizeof MsgWriteUID;
					  RequestPtr[5] = OrgPass[0];
					  RequestPtr[6] = OrgPass[1];
					  RequestPtr[7] = OrgPass[2];
					  RequestPtr[8] = OrgPass[3];
					  XorByte = CalculateCheckByte(RequestPtr, RequestLen);
					  RequestPtr[RequestLen - 1] = XorByte;
					  RequetsSent++;
					  HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
				  }
			  }
			  break;
		  }
	  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* USART1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
		// Write received byte to ReceiveBuffer
		RB_Write(&ReceiveBuffer, ReceiveTmp);
		// Start to listening UART again - important!
		HAL_UART_Receive_IT(&huart1, &ReceiveTmp, 1);
	}
}

uint8_t CalculateCheckByte(const uint8_t *message, uint8_t length)
{
	  //Calculate XOR XorByte byte: result of XOR all bytes except protocol header (0xAB, 0xBA)
	  length--; // Exclude the last byte - XorByte byte
	  uint8_t XorByte = message[2];
	  for(uint8_t i = 3; i < length; i++)
	  {
 		  XorByte = XorByte ^ message[i];
	  }

	  return XorByte;
}

uint8_t hex2uint(char MSB, char LSB)
{
	uint8_t b = 0;

    if ((MSB >= '0') && (MSB <= '9'))
        b = MSB - '0';
    else if ((MSB >= 'A') && (MSB <= 'F'))
        b = (MSB -'A') + 10;
    else if ((MSB >= 'a') && (MSB <= 'f'))
        b = (MSB - 'a') + 10;
    else
        return 0;

    b = b << 4;


	if ((LSB >= '0') && (LSB <= '9'))
		b = b | (LSB - '0');
	else if ((LSB >= 'A') && (LSB <= 'F'))
		b = b | ((LSB - 'A') + 10);
	else if ((LSB >= 'a') && (LSB <= 'f'))
		b = b | ((LSB - 'a') + 10);
	else
		return 0;

    return b;
}

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

#ifdef  USE_FULL_ASSERT
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

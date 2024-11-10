/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file       : main.c
  * @author     : MarceliKu
  * @brief      : A program to experiment with the 7941W module
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
#define NO_OF_BLOCKS 4
#define NO_OF_SECTORS 16
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

// RFID communication (USART1)
RingBuffer_t ReceiveBuffer;
uint8_t      ReceiveTmp;
extern char stringBuffer[525];

// Console communication (LPUART1)
uint8_t ConReceiveTmp; // Temporary variable for receiving one byte
volatile uint8_t ConByteReceived;
volatile uint8_t IsConSthReceived = 0;
char CurrentMode = '0';
char EchoStr[4];

// Default password
uint8_t DefPass[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Auxiliary buffer for user responses
uint8_t UserAnswers[4];

// Auxiliary buffer for block contents used in case of command 0x13 - write specified sector
uint8_t BlockContent[] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0};

// Universal buffer to be used as ID, UID or new password:
// only the first 5 bytes will be used as the ID
// only the first 4 bytes will be used as the UID
// all 6 bytes will be used as a new/user password
uint8_t UniPass[] = {0X01, 0X02, 0X03, 0X04, 0X05, 0X06}; // default universal password/ID/UID
uint8_t InputByteNo = 0; // Number of byte entered by the user on console
uint8_t ExpectedByteNo = 0; // Expected byte number

// Variables needed for memory dump
uint8_t Sector = 0;
uint8_t Block = 0;

// Software timers
uint32_t TimerMsgSend;   // Software timer for sending next message by UART

//uint8_t Sequence = 0;

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
  // experimental variant of 0x11, but also does not work :-(
  // uint8_t MsgWriteUID[]   = { 0xAB, 0xBA, 0x00, 0x11, 0x0a, 0x01, 0x02, 0x03, 0x04, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF1};  // 0x11 requests writing new Buffer (4 bytes)

  uint8_t MsgReadSector[] = { 0xAB, 0xBA, 0x00, 0x12, 0x09, 0x00, 0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10}; // 0x12 read specified sector
  uint8_t MsgWriteSector[]= { 0xAB, 0xBA, 0x00, 0x13, 0x19, 0x00, 0x01, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}; // 0x13 write specified sector
  uint8_t MsgModifyPass[] = { 0xAB, 0xBA, 0x00, 0x14, 0x0E, 0x00, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x17}; // 0x14 modify the password of group A or group B
  // Below request (0x17 - read all sector data) seams to not work.
  uint8_t MsgReadAll[]    = { 0xAB, 0xBA, 0x00, 0x17, 0x07, 0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1A};  // 0x17 read all sector data (M1-1K card)

  uint8_t* RequestPtr;
  uint8_t RequestLen;
  uint8_t XorByte;
  char    IdString[17];
  uint8_t BeginOfIdLoc = 15;
  uint8_t IdLen = 14;
  uint8_t UIdLen = 11;

  // Prepare constant part of EchoStr
  EchoStr[1] = '\n';
  EchoStr[2] = '\r';
  EchoStr[3] = '\0';

  // Start by displaying menu
  CurrentMode = 'm';
  ShowMenu();

  // Start listening to UART
  HAL_UART_Receive_IT(&huart1, &ReceiveTmp, 1);
  // Start listening to LPUART
  HAL_UART_Receive_IT(&hlpuart1, &ConReceiveTmp, 1);


  // Initiate TimerMsgSend
  TimerMsgSend   = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  //=======================================================================================================
      // Parse byte received from 7941W (UART1)
	  //=======================================================================================================
      ParseOutcome = Parse_Byte(&ReceiveBuffer);
      if(ParseOutcome == PRS_MSG_RECEIVED)
      {
     	 switch(CurrentMode)
     	 {
     	 case '1': // Periodically read Tag ID (125 kHz)
             // If it was positive (0x81) response get Id from it and send to "console" (LPUART1)
             if(stringBuffer[9] == '8' && stringBuffer[10] == '1')
        	 {
            	strncpy(IdString, stringBuffer + BeginOfIdLoc, IdLen); // extract Id from stringBuffer
            	IdString[14] = '\n';
            	IdString[15] = '\r';
            	IdString[16] = '\0';
            	HAL_UART_Transmit(&hlpuart1, (uint8_t *)IdString, strlen(IdString), 1000);

            	// Toggle LED2 if it was ID stored in first 5 bytes of UniPass
            	TextLen = sprintf((char *)Text, "%02X:%02X:%02X:%02X:%02X\n\r", UniPass[0], UniPass[1], UniPass[2], UniPass[3], UniPass[4] );
            	if(strcmp((char *)Text, IdString) == 0)
            	{
                	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
            	}
             }
     	     break;

     	 case '2': // Periodically read Tag Buffer (13.56 MHz)
             // If it was positive (0x81) response get UId from it and send to "console" (LPUART1)
             if(stringBuffer[9] == '8' && stringBuffer[10] == '1')
        	 {
            	strncpy(IdString, stringBuffer + BeginOfIdLoc, UIdLen); // extract UId from stringBuffer
            	IdString[11] = '\n';
            	IdString[12] = '\r';
            	IdString[13] = '\0';
            	HAL_UART_Transmit(&hlpuart1, (uint8_t *)IdString, strlen(IdString), 1000);

            	// Toggle LED2 if it was UID stored in first 4 bytes of UniPass
            	TextLen = sprintf((char *)Text, "%02X:%02X:%02X:%02X\n\r", UniPass[0], UniPass[1], UniPass[2], UniPass[3] );
            	if(strcmp((char *)Text, IdString) == 0)
            	{
                	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
            	}
             }
     	     break;

     	 case '4': // Write new Tag ID (125 kHz)
     	 case '5': // Write new Tag UID (13.56 MHz)
             // Send any answer to "console" (LPUART1) - positive (0x81) or negative (0x80) confirmation
     		 HAL_UART_Transmit(&hlpuart1, (uint8_t *)stringBuffer, strlen(stringBuffer), 1000);
     	     break;

     	 case '6': // Read specified sector (13.56 MHz)
     	 case '7': // Write specified sector (13.56 MHz)
     	 case '8': // Modify sector's password (13.56 MHz)
     	 case '9': // Dump group memory (13.56 MHz)
     	 case 'a': // Read all sector data (13.56 MHz)
     		 // Send any answer to "console" (LPUART1) - positive (0x81) or negative (0x80) confirmation
         	 HAL_UART_Transmit(&hlpuart1, (uint8_t *)stringBuffer, strlen(stringBuffer), 1000);
     		 break;
     	 }
		 // get ready for new message
		 Reset_Buffers();
      }

      //=======================================================================================================
      // Parse byte received from Console (LPUART1)
      //=======================================================================================================

      if(IsConSthReceived == 1) // do only if new byte has been received from console
      {
    	  IsConSthReceived = 0;
    	  switch(CurrentMode)
    	  {
    	  case '3': // Set bytes of UniPass
    		  if(InputByteNo < 6)
    		  {
    			  UniPass[InputByteNo] = ConByteReceived;
    			  InputByteNo++;
    			  // Send echo of received byte to console
    			  TextLen = sprintf((char *)Text, "%02X\n\r", ConByteReceived);
    			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    		  }
    		  break;
    	  case '6': // Read specified sector (13.56 MHz)
    	  case '7': // Write specified sector (13.56 MHz)
    		  // in consecutive steps input: sector (byte 0), block (byte 1), group (byte 2) and point out current pass (byte 3)
    		  if(InputByteNo < 4)
    		  {
    			  UserAnswers[InputByteNo] = ConByteReceived;
    			  InputByteNo++;
    			  // Send echo of received byte to console
    			  TextLen = sprintf((char *)Text, "%02X\n\r", ConByteReceived);
    			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    		  }
    		  break;
    	  case '8': // Modify sector's password (13.56 MHz)
    		  // in consecutive steps input: sector (byte 0), group (byte 1), then choose current pass (byte 2) and choose new pass (byte 3)
    		  if(InputByteNo < 4)
    		  {
    			  UserAnswers[InputByteNo] = ConByteReceived;
    			  InputByteNo++;
    			  // Send echo of received byte to console
    			  TextLen = sprintf((char *)Text, "%02X\n\r", ConByteReceived);
    			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    		  }
    		  break;
    	  case '9': // Dump group memory (13.56 MHz)
    	  case 'a': // Read all sector data (13.56 MHz)
    		  // in consecutive steps input: group (byte 0) and then choose current pass (byte 1)
    		  if(InputByteNo < 2)
    		  {
    			  UserAnswers[InputByteNo] = ConByteReceived;
    			  InputByteNo++;
    			  // Send echo of received byte to console
    			  TextLen = sprintf((char *)Text, "%02X\n\r", ConByteReceived);
    			  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    		  }
    		  break;
    	  case 'm': // Display Menu
    		  // Send echo of received mode to console
    		  EchoStr[0] = (char)ConByteReceived;
    		  HAL_UART_Transmit(&hlpuart1, (uint8_t *)EchoStr, 3, 1000);

    		  CurrentMode = (char)ConByteReceived;
    		  break;
    	  }

    	  if((char)ConByteReceived == 'm') // || NewMode == 'm')
    	  {
    		  CurrentMode = 'm';
    		  ShowMenu();
    	  }

      }


      //=======================================================================================================
	  // Send next request
      //=======================================================================================================

      if((HAL_GetTick() - TimerMsgSend) > TIMER_SEND_NEXT_REQUEST)
	  {
    	 TimerMsgSend = HAL_GetTick();
    	 switch(CurrentMode)
    	 {
    	 case '1':
    		 RequestPtr = MsgReadID;
    		 RequestLen = sizeof MsgReadID;
    		 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    		 break;
    	 case '2':
     		 RequestPtr = MsgReadUID;
    		 RequestLen = sizeof MsgReadUID;
    		 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    		 break;
    	 case '3':
    		 if(InputByteNo < 6)
    		 {
    			 if(InputByteNo == ExpectedByteNo)
    			 {
        			 TextLen = sprintf((char *)Text, "\n\rEnter %d. byte of new (U)ID: ", ExpectedByteNo);
        			 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    				 ExpectedByteNo++;
    			 }
    		 }
   			 else
   			 {
   				CurrentMode = 'm';
   				ShowMenu();
   			 }
    		 break;
    	 case '4':
    		 if(RequetsSent < 1) // to send this request only once
    		 {
        		 TextLen = sprintf((char *)Text, "Writing new ID (5 Bytes): %02X:%02X:%02X:%02X:%02X)\n\r", UniPass[0], UniPass[1], UniPass[2], UniPass[3], UniPass[4] );
        		 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
        		 RequestPtr = MsgWriteID;
        		 RequestLen = sizeof MsgWriteID;
    	   	     RequestPtr[5] = UniPass[0];
    			 RequestPtr[6] = UniPass[1];
    	   	     RequestPtr[7] = UniPass[2];
    			 RequestPtr[8] = UniPass[3];
    	   	     RequestPtr[9] = UniPass[4];
       	    	 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
        	     RequestPtr[RequestLen - 1] = XorByte;
        	     RequetsSent++;
        	     HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    		 }
    		 break;
    	 case '5':
    		 if(RequetsSent < 1) // to send this request only once
    		 {
        		 TextLen = sprintf((char *)Text, "Writing new Buffer (4 Bytes): %02X:%02X:%02X:%02X)\n\r", UniPass[0], UniPass[1], UniPass[2], UniPass[3] );
        		 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
        		 RequestPtr = MsgWriteUID;
        		 RequestLen = sizeof MsgWriteUID;
    	   	     RequestPtr[5] = UniPass[0];
    			 RequestPtr[6] = UniPass[1];
    	   	     RequestPtr[7] = UniPass[2];
    			 RequestPtr[8] = UniPass[3];
       	    	 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
        	     RequestPtr[RequestLen - 1] = XorByte;
        	     RequetsSent++;
        	     HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    		 }
    		 break;
    	 case '6': // Read specified sector (13.56 MHz)
    		 if(InputByteNo < 4)
    		 {
        		 // First set sector (0-15), block (0-3), group (0x0A/0x0B) and password (DefPass or UniPass)
    			 if(InputByteNo == ExpectedByteNo) // send next command only after previous answer received
    			 {
        	    	 switch(ExpectedByteNo)
        	    	 {
        	    	 case 0:
            			 TextLen = sprintf((char *)Text, "Enter sector no (00-0F) : ");
        	    		 break;
        	    	 case 1:
        	    		 TextLen = sprintf((char *)Text, "Enter block no (00-03) : ");
        	    		 break;
        	    	 case 2:
        	    		 TextLen = sprintf((char *)Text, "Enter group no (0A or 0B) : ");
        	    		 break;
        	    	 case 3:
        	    		 TextLen = sprintf((char *)Text, "Which password to use as the current one? (01 - DefPass or 02 - UniPass) : ");
        	    		 break;
        	    	 }
        			 ExpectedByteNo++;
            		 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    			 }
    		 }
    		 else
    		 {
    			 if(RequetsSent < 1) // to send this request only once
    			 {
        			 RequestPtr = MsgReadSector;
        			 RequestLen = sizeof MsgReadSector;
        	   	     RequestPtr[5] = UserAnswers[0]; // Sector
        			 RequestPtr[6] = UserAnswers[1]; // Block
        			 RequestPtr[7] = UserAnswers[2]; // A or B group password (0x0A/0x0B)
        			 // insert password:
        			 if(UserAnswers[3] == 0x01) // DefPass was chosen
        			 {
            			 RequestPtr[8]  = DefPass[0];
            			 RequestPtr[9]  = DefPass[1];
            			 RequestPtr[10] = DefPass[2];
            			 RequestPtr[11] = DefPass[3];
            			 RequestPtr[12] = DefPass[4];
            			 RequestPtr[13] = DefPass[5];
        			 }
        			 else // UniPass was chosen
        			 {
            			 RequestPtr[8]  = UniPass[0];
            			 RequestPtr[9]  = UniPass[1];
            			 RequestPtr[10] = UniPass[2];
            			 RequestPtr[11] = UniPass[3];
            			 RequestPtr[12] = UniPass[4];
            			 RequestPtr[13] = UniPass[5];
        			 }

        	    	 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
        	    	 RequestPtr[RequestLen - 1] = XorByte;
    	    		 TextLen = sprintf((char *)Text, "Request sent. RFID tag response:\n\r");
          		     HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
        	    	 RequetsSent++;
        	    	 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    			 }
    		 }
    		 break;
    	 case '7': // Write specified sector (13.56 MHz)
    		 if(InputByteNo < 4)
    		 {
    			 // First set sector (0-15), block (0-3), group (0x0A/0x0B) and password (DefPass or UniPass)
    			 if(InputByteNo == ExpectedByteNo) // send next command only after previous answer received
    			 {
    				 switch(ExpectedByteNo)
    				 {
    				 case 0:
    					 TextLen = sprintf((char *)Text, "Enter sector no (00-0F) : ");
    					 break;
    				 case 1:
    					 TextLen = sprintf((char *)Text, "Enter block no (00-03) : ");
    					 break;
    				 case 2:
    					 TextLen = sprintf((char *)Text, "Enter group no (0A or 0B) : ");
    					 break;
    				 case 3:
    					 TextLen = sprintf((char *)Text, "Which password to use as the current one? (01 - DefPass or 02 - UniPass) : ");
    					 break;
    				 }
    				 ExpectedByteNo++;
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    			 }
    		 }
    		 else
    		 {
    			 if(RequetsSent < 1) // to send this request only once
    			 {
    				 RequestPtr = MsgWriteSector;
    				 RequestLen = sizeof MsgWriteSector;
        	   	     RequestPtr[5] = UserAnswers[0]; // Sector
        			 RequestPtr[6] = UserAnswers[1]; // Block
        			 RequestPtr[7] = UserAnswers[2]; // A or B group password (0x0A/0x0B)
        			 // insert password:
        			 if(UserAnswers[3] == 0x01) // DefPass was chosen
        			 {
            			 RequestPtr[8]  = DefPass[0];
            			 RequestPtr[9]  = DefPass[1];
            			 RequestPtr[10] = DefPass[2];
            			 RequestPtr[11] = DefPass[3];
            			 RequestPtr[12] = DefPass[4];
            			 RequestPtr[13] = DefPass[5];
        			 }
        			 else // UniPass was chosen
        			 {
            			 RequestPtr[8]  = UniPass[0];
            			 RequestPtr[9]  = UniPass[1];
            			 RequestPtr[10] = UniPass[2];
            			 RequestPtr[11] = UniPass[3];
            			 RequestPtr[12] = UniPass[4];
            			 RequestPtr[13] = UniPass[5];
        			 }
        			 // Insert 16 bytes content of block
        			 RequestPtr[14] = BlockContent[0];
        			 RequestPtr[15] = BlockContent[1];
        			 RequestPtr[16] = BlockContent[2];
        			 RequestPtr[17] = BlockContent[3];
        			 RequestPtr[18] = BlockContent[4];
        			 RequestPtr[19] = BlockContent[5];
        			 RequestPtr[20] = BlockContent[6];
        			 RequestPtr[21] = BlockContent[7];
        			 RequestPtr[22] = BlockContent[8];
        			 RequestPtr[23] = BlockContent[9];
        			 RequestPtr[24] = BlockContent[10];
        			 RequestPtr[25] = BlockContent[11];
        			 RequestPtr[26] = BlockContent[12];
        			 RequestPtr[27] = BlockContent[13];
        			 RequestPtr[28] = BlockContent[14];
        			 RequestPtr[29] = BlockContent[15];

    				 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
    				 RequestPtr[RequestLen - 1] = XorByte;
    				 TextLen = sprintf((char *)Text, "Request sent. RFID tag response:\n\r");
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    				 RequetsSent++;
    				 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    			 }
    		 }
    		 break;
    	 case '8': // Modify sector's password (13.56 MHz)
    		 if(InputByteNo < 4)
    		 {
    			 // First set sector (0-15), group (0x0A/0x0B), old and new passwords (DefPass or UniPass)
    			 if(InputByteNo == ExpectedByteNo) // send next command only after previous answer received
    			 {
    				 switch(ExpectedByteNo)
    				 {
    				 case 0:
    					 TextLen = sprintf((char *)Text, "Enter sector no (00-0F) : ");
    					 break;
    				 case 1:
    					 TextLen = sprintf((char *)Text, "Enter group no (0A or 0B) : ");
    					 break;
    				 case 2:
    					 TextLen = sprintf((char *)Text, "Which password to use as the current one? (01 - DefPass or 02 - UniPass) : ");
    					 break;
    				 case 3:
    					 TextLen = sprintf((char *)Text, "Which password to use as the new one? (01 - DefPass or 02 - UniPass) : ");
    					 break;
    				 }
    				 ExpectedByteNo++;
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    			 }
    		 }
    		 else
    		 {
    			 if(RequetsSent < 1) // to send this request it only once
    			 {
    				 RequestPtr = MsgModifyPass;
    				 RequestLen = sizeof MsgModifyPass;
    				 RequestPtr[5] = UserAnswers[0]; // Sector
    				 RequestPtr[6] = UserAnswers[1]; // A or B group password (0x0A/0x0B)
    				 // insert current password:
    				 if(UserAnswers[2] == 0x01) // DefPass was chosen
    				 {
    					 RequestPtr[7]  = DefPass[0];
    					 RequestPtr[8]  = DefPass[1];
    					 RequestPtr[9]  = DefPass[2];
    					 RequestPtr[10] = DefPass[3];
    					 RequestPtr[11] = DefPass[4];
    					 RequestPtr[12] = DefPass[5];
    				 }
    				 else // UniPass was chosen
    				 {
    					 RequestPtr[7]  = UniPass[0];
    					 RequestPtr[8]  = UniPass[1];
    					 RequestPtr[9]  = UniPass[2];
    					 RequestPtr[10] = UniPass[3];
    					 RequestPtr[11] = UniPass[4];
    					 RequestPtr[12] = UniPass[5];
    				 }
    				 // insert new password:
    				 if(UserAnswers[3] == 0x01) // DefPass was chosen
    				 {
    					 RequestPtr[13] = DefPass[0];
    					 RequestPtr[14] = DefPass[1];
    					 RequestPtr[15] = DefPass[2];
    					 RequestPtr[16] = DefPass[3];
    					 RequestPtr[17] = DefPass[4];
    					 RequestPtr[18] = DefPass[5];
    				 }
    				 else // UniPass was chosen
    				 {
    					 RequestPtr[13] = UniPass[0];
    					 RequestPtr[14] = UniPass[1];
    					 RequestPtr[15] = UniPass[2];
    					 RequestPtr[16] = UniPass[3];
    					 RequestPtr[17] = UniPass[4];
    					 RequestPtr[18] = UniPass[5];
    				 }
    				 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
    				 RequestPtr[RequestLen - 1] = XorByte;
    				 TextLen = sprintf((char *)Text, "Request sent. RFID tag response:\n\r");
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    				 RequetsSent++;
    				 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);
    			 }
    		 }
    		 break;

    	 case '9': // Dump group memory (13.56 MHz)
    		 if(InputByteNo < 2)
    		 {
    			 // First set group (0x0A or 0x0B) and password (DefPass or UniPass)
    			 if(InputByteNo == ExpectedByteNo) // send next command only after previous answer received
    			 {
    				 switch(ExpectedByteNo)
    				 {
    				 case 0:
    					 TextLen = sprintf((char *)Text, "Enter group no (0A or 0B) : ");
    					 break;
    				 case 1:
    					 TextLen = sprintf((char *)Text, "Which password to use as the current one? (01 - DefPass or 02 - UniPass) : ");
    					 break;
    				 }
    				 ExpectedByteNo++;
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    			 }
    		 }
    		 else
    		 {
    			 if(Sector < NO_OF_SECTORS)
    			 {
    				 RequestPtr = MsgReadSector;
    				 RequestLen = sizeof MsgReadSector;

           	   	     RequestPtr[5] = Sector;
            		 RequestPtr[6] = Block;
            		 RequestPtr[7] = UserAnswers[0]; // A or B group password (0x0A/0x0B)
            		 // insert password:
					 if(UserAnswers[1] == 0x01) // DefPass was chosen
					 {
						 RequestPtr[8]  = DefPass[0];
						 RequestPtr[9]  = DefPass[1];
						 RequestPtr[10] = DefPass[2];
						 RequestPtr[11] = DefPass[3];
						 RequestPtr[12] = DefPass[4];
						 RequestPtr[13] = DefPass[5];
					 }
					 else // UniPass was chosen
					 {
						 RequestPtr[8]  = UniPass[0];
						 RequestPtr[9]  = UniPass[1];
						 RequestPtr[10] = UniPass[2];
						 RequestPtr[11] = UniPass[3];
						 RequestPtr[12] = UniPass[4];
						 RequestPtr[13] = UniPass[5];
					 }

					 XorByte = CalculateCheckByte(RequestPtr, RequestLen);
					 RequestPtr[RequestLen - 1] = XorByte;


    				 HAL_UART_Transmit(&huart1, RequestPtr, RequestLen, 500);

    				 Block = (Block + 1) % NO_OF_BLOCKS; // increment Block but after SECTOR_SIZE reset to 0

    				 if(Block == 0) // increment Sector but only when Block reset is equal 0
    				 {
    					 Sector++;
    				 }
    			 }
    		 }
    		 break;
    	 case 'a': // Read all sector data (13.56 MHz)
    		 if(InputByteNo < 2)
    		 {
    			 // First set group (0x0A or 0x0B) and password (DefPass or UniPass)
    			 if(InputByteNo == ExpectedByteNo) // send next command only after previous answer received
    			 {
    				 switch(ExpectedByteNo)
    				 {
    				 case 0:
    					 TextLen = sprintf((char *)Text, "Enter group no (0A or 0B) : ");
    					 break;
    				 case 1:
    					 TextLen = sprintf((char *)Text, "Which password to use as the current one? (01 - DefPass or 02 - UniPass) : ");
    					 break;
    				 }
    				 ExpectedByteNo++;
    				 HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
    			 }
    		 }
    		 else
    		 {
        		 if(RequetsSent < 1) // to send this request it only once
        		 {
        			 RequestPtr = MsgReadAll;
        			 RequestLen = sizeof MsgReadAll;

            		 RequestPtr[5] = UserAnswers[0]; // A or B group password (0x0A/0x0B)
            		 // insert password:
					 if(UserAnswers[1] == 0x01) // DefPass was chosen
					 {
						 RequestPtr[6]  = DefPass[0];
						 RequestPtr[7]  = DefPass[1];
						 RequestPtr[8]  = DefPass[2];
						 RequestPtr[9]  = DefPass[3];
						 RequestPtr[10] = DefPass[4];
						 RequestPtr[11] = DefPass[5];
					 }
					 else // UniPass was chosen
					 {
						 RequestPtr[6]  = UniPass[0];
						 RequestPtr[7]  = UniPass[1];
						 RequestPtr[8]  = UniPass[2];
						 RequestPtr[9]  = UniPass[3];
						 RequestPtr[10] = UniPass[4];
						 RequestPtr[11] = UniPass[5];
					 }

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
	else if(huart->Instance == LPUART1)
	{
		ConByteReceived = ConReceiveTmp;
		IsConSthReceived = 1;
		// Start to listening UART again - important!
		HAL_UART_Receive_IT(&hlpuart1, &ConReceiveTmp, 1);
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

void ShowMenu()
{
	  TextLen = sprintf((char *)Text, "\n\rMenu options:\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "1 - Periodically read Tag ID (125 kHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "2 - Periodically read Tag Buffer (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

	  TextLen = sprintf((char *)Text, "3 - Set bytes of UniPass (current: %02X:%02X:%02X:%02X:%02X:%02X)\n\r", UniPass[0], UniPass[1], UniPass[2], UniPass[3], UniPass[4], UniPass[5] );
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

	  TextLen = sprintf((char *)Text, "4 - Write new Tag ID (125 kHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "5 - Write new Tag UID (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);


	  TextLen = sprintf((char *)Text, "6 - Read specified sector (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "7 - Write specified sector (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "8 - Modify sector's password (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

	  TextLen = sprintf((char *)Text, "9 - Dump group memory (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "a - Read all sector data (13.56 MHz)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

	  TextLen = sprintf((char *)Text, "m - Display Menu (any time)\n\r");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);
	  TextLen = sprintf((char *)Text, "\n\r>Send chosen option ('1'-'9' or 'a' or 'm'): ");
	  HAL_UART_Transmit(&hlpuart1, (uint8_t*)Text, TextLen, 1000);

	  // Reset variables related to some modes
	  Sector = 0;
	  Block = 0;
	  RequetsSent = 0;
	  InputByteNo = 0;
	  ExpectedByteNo = 0;
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

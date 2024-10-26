/*
 * parser.c
 *
 *  Created on: 6 cze 2024
 *      Author: mkuli
 */

#include "main.h"
#include "ring_buffer.h"
#include "parser.h"
#include "stdio.h"
#include "string.h"
#include "usart.h"

char stringBuffer[525];
char* endStrPtr = stringBuffer;
uint8_t  CurrentMsg[262];
uint8_t  CurPosition = 0;
uint8_t  CurByte;
uint8_t  DataBytesToRead = 0;


PRS_Outcome Parse_Byte(RingBuffer_t *RingBuffer)
{
	  if(RingBuffer->Head != RingBuffer->Tail) // if there is at least one byte to read in ring buffer
	  {
		 RB_Read(RingBuffer, &CurByte);

		 if(CurPosition == 0) // it means that we are waiting for new message
		 {
			 if(CurByte == 0xCD) // correct first byte of response message header
			 {
				 CurrentMsg[0] = CurByte;
				 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
				 CurPosition++;
			 }
			 else
			 {
				 // skip this byte or save it in additional buffer for damaged messages
				 return PRS_ERROR;
			 }
		 }
		 else if(CurPosition == 1)
		 {
			 if (CurByte == 0xDC) // correct second byte of response message header
			 {
				 CurrentMsg[1] = CurByte;
				 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
				 CurPosition++;
			 }
			 else
			 {
				 // skip this byte or save it in buffer for damaged messages
				 // reset position in buffer for message and buffer for its string representation
				 CurPosition = 0;
				 endStrPtr = stringBuffer;
			 }
		 }
		 else if (CurPosition == 2) // address
		 {
			 CurrentMsg[2] = CurByte;
			 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
			 CurPosition++;
		 }
		 else if (CurPosition == 3) // command (0x80 = failed or 0x81 = success)
		 {
			 CurrentMsg[3] = CurByte;
			 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
			 CurPosition++;
		 }
		 else if (CurPosition == 4) // data length
		 {
			 CurrentMsg[4] = CurByte;
			 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
			 DataBytesToRead = CurByte;
			 CurPosition++;
		 }
		 else if (DataBytesToRead == 0) // XOR check byte
		 {
			 CurrentMsg[CurPosition] = CurByte;
			 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
			 CurPosition++;
			 // End of message! Send it to "console" (LPUART1)
			 endStrPtr += sprintf(endStrPtr, "\n\r");

			 return PRS_MSG_RECEIVED;
		 }
		 else
		 {
			 CurrentMsg[CurPosition] = CurByte;
			 endStrPtr += sprintf(endStrPtr, "%02X:", CurByte);
			 DataBytesToRead--;
			 CurPosition++;
		 }

		 return PRS_BYTE_OK;
	  }
	  else
	  {
		  return PRS_NO_BYTE;
	  }

}

void Reset_Buffers()
{
	CurPosition = 0;
	endStrPtr = stringBuffer;
}

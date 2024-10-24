/*
 * ring_buffer.c
 *
 *  Created on: Jun 11, 2023
 *      Author: mkuli based on code of Mateusz Salamon
 */

#include "main.h"
#include "ring_buffer.h"

RB_Status RB_Read(RingBuffer_t *Buf, uint8_t *Value)
{
    if(Buf->Head == Buf->Tail)
    {
    	// there is nothing to read
    	return RB_ERROR;
    }
    // Ok, we can read next value
    *Value = Buf->Buffer[Buf->Tail];
	Buf->Tail = (Buf->Tail + 1) % RING_BUFFER_SIZE;

	return RB_OK;
}

RB_Status RB_Write(RingBuffer_t *Buf, uint8_t Value)
{
	uint16_t HeadTmp = (Buf->Head + 1) % RING_BUFFER_SIZE; // thanks to modulo division, we return to the beginning
	                                                       // when the end of the buffer is exceeded
    if(HeadTmp == Buf->Tail)
    {
    	return RB_ERROR;
    }
	Buf->Buffer[Buf->Head] = Value;
    Buf->Head = HeadTmp;
	return RB_OK;
}

void RB_Flush(RingBuffer_t *Buf)
{
	Buf->Head = 0;
	Buf->Tail = 0;
}

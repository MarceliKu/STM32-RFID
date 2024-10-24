/*
 * ring_buffer.h
 *
 *  Created on: Jun 11, 2023
 *      Author: mkuli
 */

#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

#define RING_BUFFER_SIZE 262 // the longest message according to protocol

// Success status
typedef enum
{
	RB_OK    = 0,
	RB_ERROR = 1
} RB_Status;

// Object Ping Buffer
typedef struct
{
	uint16_t Head;
	uint16_t Tail;
	uint8_t  Buffer[RING_BUFFER_SIZE];
} RingBuffer_t;

// Functions:
// Write
RB_Status RB_Write(RingBuffer_t *Buf, uint8_t Value);

// Read
RB_Status RB_Read(RingBuffer_t *Buf, uint8_t *Value);

// Flush
void RB_Flush(RingBuffer_t *Buf);

#endif /* INC_RING_BUFFER_H_ */

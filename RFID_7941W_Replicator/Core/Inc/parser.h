/*
 * parser.h
 *
 *  Created on: 6 cze 2024
 *      Author: mkuli
 */

#ifndef INC_PARSER_H_
#define INC_PARSER_H_

// Type for outcome of parser actions
typedef enum
{
	PRS_NO_BYTE    = 0, // No new byte to read
	PRS_BYTE_OK    = 1, // First/Next byte parsed correctly
	PRS_ERROR = 2,      // Received byte do not fit to the protocol
	PRS_MSG_RECEIVED = 3 // Whole message received!
} PRS_Outcome;

PRS_Outcome Parse_Byte(RingBuffer_t *RingBuffer);
void Reset_Buffers();

#endif /* INC_PARSER_H_ */

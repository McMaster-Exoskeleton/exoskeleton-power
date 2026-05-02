/*
 * circular_buffer.c
 *
 * Generic circular buffer implementation for float values.
 * Stores the last CIRC_BUF_SIZE samples and maintains a running average
 * that is recomputed on every push. Used by the telemetry module to
 * smooth INA228 voltage and current readings before CAN transmission.
 */

#include "circular_buffer.h"
#include <string.h>

void circ_buf_init(CircularBuffer_t *cb)
{
    memset(cb, 0, sizeof(CircularBuffer_t)); // Reset buffer by zeroing memory block
}

void circ_buf_push(CircularBuffer_t *cb, float value)
{
	cb->buf[cb->index] = value; 					// Write into the current slot
	cb->index = (cb->index + 1) % CIRC_BUF_SIZE; 	// Advance write pointer and wrap around

	if(cb->count < CIRC_BUF_SIZE) cb->count++; 		// Increase sample count until buffer is full

    /* Recompute average over all valid slots */
    float sum = 0.0f;
    for (int i = 0; i < cb->count; i++)
    	sum += cb->buf[i];
    cb->average = sum / (float)cb->count;
}

float circ_buf_average(const CircularBuffer_t *cb)
{
    return cb->average;
}

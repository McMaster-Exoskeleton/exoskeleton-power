/*
 * circular_buffer.h
 *
 *  Created on: Apr 4, 2026
 *      Author: EmChan
 *
 *  Generic circular buffer for float values.
 *  Maintains a rolling average over the last CIRC_BUF_SIZE samples.
 *
 */

#ifndef INC_CIRCULAR_BUFFER_H_
#define INC_CIRCULAR_BUFFER_H_

#include <stdint.h>

// Number of samples in each rolling window.
// At a 100ms poll rate, 10 samples = latest 1s of data
#define CIRC_BUF_SIZE   10

typedef struct {
	float buf[CIRC_BUF_SIZE];	// Array for storing samples
	uint8_t index;				// Next write position (wraps around)
	uint8_t count;				// Number of samples in circular buffer
	float average; 				// Most recently computed rolling average
} CircularBuffer_t;


/* Clear all samples and reset state */
void circ_buf_init(CircularBuffer_t *cb);

/* Push a new sample in and recompute the average */
void circ_buf_push(CircularBuffer_t *cb, float value);

/* Return the current rolling average (0.0f if no samples yet) */
float circ_buf_average(const CircularBuffer_t *cb);

#endif /* INC_CIRCULAR_BUFFER_H_ */

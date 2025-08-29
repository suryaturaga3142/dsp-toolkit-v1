/*
 * mode_hilbert.h
 *
 *  Created on: Aug 27, 2025
 *      Author: surya
 */

#ifndef INC_MODE_HILBERT_H_
#define INC_MODE_HILBERT_H_

#include "arm_math.h"
#include "globals.h"
#include "wavegen.h"

#define FFT_SIZE       (ADC_BUF_LEN/2)    // 1024
#define NUM_BINS       (FFT_SIZE/2)       // 512
#define UART_BUF_LEN   (NUM_BINS * 6 + 50) // Room for formatting "Start ...", spaces, values, CRLF

void Hilbert_Init(void);
void Hilbert_procData(volatile uint16_t *adc_data, int st_idx);
void Hilbert_printDataDMA(uint16_t *envelope_data);
void myDSP_hilbert_f32(float32_t* input, float32_t* output);

#endif /* INC_MODE_HILBERT_H_ */

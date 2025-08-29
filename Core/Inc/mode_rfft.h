/*
 * mode_rfft.h
 *
 *  Created on: Aug 14, 2025
 *      Author: surya
 */

#ifndef INC_MODE_RFFT_H_
#define INC_MODE_RFFT_H_

#include "main.h"
#include "globals.h"
#include "wavegen.h"

#define FFT_SIZE       (ADC_BUF_LEN/2)    // 1024
#define NUM_BINS       (FFT_SIZE/2)       // 512
#define UART_BUF_LEN   (NUM_BINS * 6 + 50) // Room for formatting "Start ...", spaces, values, CRLF

void RFFT_Init(void);
void RFFT_procData(volatile uint16_t adc_buf[], int st_idx);
void RFFT_printDataDMA(uint16_t *magnitude);

#endif /* INC_MODE_RFFT_H_ */

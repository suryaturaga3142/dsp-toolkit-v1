/*
 * mode_cfft.h
 *
 *  Created on: Aug 15, 2025
 *      Author: surya
 */

#ifndef INC_MODE_CFFT_H_
#define INC_MODE_CFFT_H_

#include "main.h"
#include "globals.h"

#define FFT_SIZE       ADC_BUF_LEN    // 2048
#define NUM_BINS       FFT_SIZE       // 2048 bc no Nyquist
#define UART_BUF_LEN   (NUM_BINS * 8 + 50) // Room for formatting "Start ...", spaces, magnitude, phase, CRLF

void CFFT_Init(void);

void CFFT_procData(volatile uint16_t adc_buf[], int st_idx);
void CFFT_printDataDMA(uint16_t *packed);

#endif /* INC_MODE_CFFT_H_ */

/*
 * mode_cepstrum.h
 *
 *  Created on: Aug 15, 2025
 *      Author: surya
 */

#ifndef INC_MODE_CEPSTRUM_H_
#define INC_MODE_CEPSTRUM_H_

#include "arm_math.h"
#include "app_config.h"
#include "globals.h"

#define FFT_SIZE       (ADC_BUF_LEN/2)    // 1024
#define NUM_BINS       (FFT_SIZE/2)       // 512
#define UART_BUF_LEN   (NUM_BINS * 6 + 50) // Room for formatting "Start ...", spaces, values, CRLF

// Public function prototypes
void Cepstrum_Init(void);
void Cepstrum_procData(volatile uint16_t *adc_data, int st_idx);
void Cepstrum_printDataDMA(uint16_t *cepstrum_data);

#endif /* INC_MODE_CEPSTRUM_H_ */

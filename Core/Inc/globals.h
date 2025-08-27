/*
 * globals.h
 *
 *  Created on: Aug 15, 2025
 *      Author: surya
 */

#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_

#include "main.h"

#define TIM_CLK        96000000
#define SAMPLE_FREQ    40000
#define DAC_BUF_LEN    1024
#define ADC_BUF_LEN    2048

extern volatile uint16_t adc_buf[ADC_BUF_LEN];
extern volatile uint16_t dac_buf[DAC_BUF_LEN];
extern volatile uint8_t uart3_busy;
extern volatile uint8_t uart_buf_index_tx;   // buffer currently being transmitted
extern volatile uint8_t uart_buf_index_fill; // buffer being filled


#endif /* INC_GLOBALS_H_ */

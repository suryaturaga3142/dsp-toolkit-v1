/*
 * globals.c
 *
 *  Created on: Aug 15, 2025
 *      Author: surya
 */

#include "globals.h"

volatile uint16_t adc_buf[ADC_BUF_LEN];
volatile uint16_t dac_buf[DAC_BUF_LEN];
volatile uint8_t uart3_busy = 0;
volatile uint8_t uart_buf_index_tx = 0;   // buffer currently being transmitted
volatile uint8_t uart_buf_index_fill = 1; // buffer being filled

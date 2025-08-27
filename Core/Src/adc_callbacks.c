/*
 * adc_callbacks.c
 *
 *  Created on: Aug 14, 2025
 *      Author: surya
 */

#include "app_config.h"
#include "main.h"
#include "globals.h"

#ifdef MODE_RFFT
#include "mode_rfft.h"
#endif
#ifdef MODE_CFFT
#include "mode_cfft.h"
#endif
#ifdef MODE_CEPSTRUM
#include "mode_cepstrum.h"
#endif

#ifdef MODE_RFFT

// ADC DMA half complete callback
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        RFFT_procData(adc_buf, 0);
    }
}

// ADC DMA full complete callback
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        RFFT_procData(adc_buf, (ADC_BUF_LEN / 2));
    }
}

// UART DMA transmit complete callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        uart3_busy = 0;

        // Swap TX and fill buffer indices for next transmission
        uart_buf_index_tx = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;
    }
}

#endif

#ifdef MODE_CFFT

// ADC DMA half complete callback
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        CFFT_procData(adc_buf, 0);
    }
}

// ADC DMA full complete callback
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) {
        CFFT_procData(adc_buf, (ADC_BUF_LEN / 2));
    }
}

// UART DMA transmit complete callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        uart3_busy = 0;

        // Swap TX and fill buffer indices for next transmission
        uart_buf_index_tx = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;
    }
}

#endif

#ifdef MODE_CEPSTRUM

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
    if(hadc->Instance == ADC1) {
    	Cepstrum_procData(adc_buf, 0);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if(hadc->Instance == ADC1) {
    	Cepstrum_procData(adc_buf, FFT_SIZE);
    }
}

// UART DMA transmit complete callback
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        uart3_busy = 0;

        // Swap TX and fill buffer indices for next transmission
        uart_buf_index_tx = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;
    }
}


#endif

/*
 * mode_cfft.c
 *
 *  Created on: Aug 15, 2025
 *      Author: surya
 */

#include "main.h"
#include "mode_cfft.h"

static arm_cfft_instance_f32 cfft_instance;

// Double buffers for UART DMA transmission
static uint8_t uart_buf[2][UART_BUF_LEN];

// Hann windowing
static float hann_window[FFT_SIZE];

void CFFT_Init(void) {

	for (int i = 0; i < FFT_SIZE; i++) {
		 hann_window[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i) / (FFT_SIZE - 1)));
	}

	arm_cfft_init_f32(&cfft_instance, FFT_SIZE);

	HAL_TIM_Base_Start(&htim2);

	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN);

}


// FFT processing and UART buffer filling
void CFFT_procData(volatile uint16_t *adc_data, int st_idx) {

    float input[FFT_SIZE];
    float mag_f32[NUM_BINS];
    float phase_f32[NUM_BINS];
    static uint16_t packed_u16[NUM_BINS * 2];

    // Convert ADC samples to normalized float and apply Hanning window
    for (int i = 0; i < FFT_SIZE; i++) {
        float normalized = (float)(adc_data[st_idx + i]) / 4096.0f;
        input[i] = normalized * hann_window[i];
    }

    arm_cfft_init_f32(&cfft_instance, FFT_SIZE);

    // Compute magnitude and phase for each bin
    for (int i = 0; i < NUM_BINS; i++) {
        float real = input[2 * i];
        float imag = input[2 * i + 1];
        mag_f32[i] = sqrtf(real * real + imag * imag);
        phase_f32[i] = atan2f(imag, real); // in radians
    }

    // Scale and pack magnitude-phase into uint16_t array
    for (int i = 0; i < NUM_BINS; i++) {
    	float mag_scaled = mag_f32[i] * 1000.0f; // adjust scaling
        if (mag_scaled > 65535.0f) mag_scaled = 65535.0f;
        packed_u16[i * 2] = (uint16_t)mag_scaled;

        // Map phase [-pi, pi] to [0, 65535]
        float phase_norm = (phase_f32[i] + (float)M_PI) / (2.0f * (float)M_PI);
        uint16_t phase_u16 = (uint16_t)(phase_norm * 65535.0f);
        packed_u16[i * 2 + 1] = phase_u16;
    }

    // Send over UART DMA
    CFFT_printDataDMA(packed_u16);
}

void CFFT_printDataDMA(uint16_t *packed_u16) {
	uint8_t *fill_buf = uart_buf[uart_buf_index_fill];
    int len = 0;

    // Header
    fill_buf[len++] = 0xBB; // start byte
    fill_buf[len++] = (uint8_t)(NUM_BINS & 0xFF);
    fill_buf[len++] = (uint8_t)((NUM_BINS >> 8) & 0xFF);
    fill_buf[len++] = (uint8_t)(SAMPLE_FREQ & 0xFF);
    fill_buf[len++] = (uint8_t)((SAMPLE_FREQ >> 8) & 0xFF);

    // Data: each bin has magnitude and phase (uint16_t each, little-endian)
    for (int i = 0; i < NUM_BINS * 2; i++) {
        fill_buf[len++] = (uint8_t)(packed_u16[i] & 0xFF);
        fill_buf[len++] = (uint8_t)((packed_u16[i] >> 8) & 0xFF);
    }

	// Footer
    fill_buf[len++] = 0x22;

    if (!uart3_busy) {
        uart3_busy = 1;
        uart_buf_index_tx = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;
        HAL_UART_Transmit_DMA(&huart3, fill_buf, len);
    }
	// If busy, the frame will remain in fill buffer until callback swaps it
    return;
}

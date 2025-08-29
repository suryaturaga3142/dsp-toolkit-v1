/*
 * mode_rfft.c
 *
 *  Created on: Aug 14, 2025
 *      Author: surya
 */

#include "main.h"
#include "mode_rfft.h"

//volatile uint16_t dac_buf[DAC_BUF_LEN];

static arm_rfft_fast_instance_f32 fft_instance;

// Double buffers for UART DMA transmission
static uint8_t uart_buf[2][UART_BUF_LEN];

// Hann windowing
static float hann_window[FFT_SIZE];

void RFFT_Init(void) {

	Wavegen_Init(dac_buf);

	for (int i = 0; i < FFT_SIZE; i++) {
		 hann_window[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i) / (FFT_SIZE - 1)));
	}

	arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

	HAL_TIM_Base_Start(&htim2);

	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN);

}


// FFT processing and UART buffer filling
void RFFT_procData(volatile uint16_t *adc_data, int st_idx) {

    float input[FFT_SIZE];
    float output[FFT_SIZE];
    float mag_f32[NUM_BINS];
    static uint16_t mag_u16[NUM_BINS];

    // Convert ADC samples to normalized float and apply Hanning window
    for (int i = 0; i < FFT_SIZE; i++) {
        float normalized = (float)(adc_data[st_idx + i]) / 4096.0f;
        input[i] = normalized * hann_window[i];
    }

    arm_rfft_fast_f32(&fft_instance, input, output, 0);

    arm_cmplx_mag_f32(output, mag_f32, NUM_BINS);

    // Scale magnitudes to 16-bit unsigned ints (e.g., scale factor = 1000)
    for (int i = 0; i < NUM_BINS; i++) {
        float scaled = mag_f32[i] * 1000.0f;
        if (scaled > 65535.0f) scaled = 65535.0f;
        mag_u16[i] = (uint16_t)scaled;
    }

    // Format and prepare magnitude data for DMA transfer
    RFFT_printDataDMA(mag_u16);
}

void RFFT_printDataDMA(uint16_t *magnitude) {
    // Pointer to the current fill buffer in our 2-buffer system
    uint8_t *fill_buf = uart_buf[uart_buf_index_fill];
    int len = 0;

    // Example header (optional) - 2 bytes: marker and bin count
    // You can define any binary header structure you want
    fill_buf[len++] = 0xAA;                   // Start byte marker
    fill_buf[len++] = (uint8_t)(NUM_BINS & 0xFF);      // Number of bins
    fill_buf[len++] = (uint8_t)((NUM_BINS >> 8) & 0xFF);
    fill_buf[len++] = (uint8_t)(SAMPLE_FREQ & 0xFF); //Sampling frequency
    fill_buf[len++] = (uint8_t)((SAMPLE_FREQ >> 8) & 0xFF);


    // Copy raw uint16_t magnitudes into buffer (little-endian)
    for (int i = 0; i < NUM_BINS; i++) {
        fill_buf[len++] = (uint8_t)(magnitude[i] & 0xFF);       // LSB
        fill_buf[len++] = (uint8_t)((magnitude[i] >> 8) & 0xFF); // MSB
    }

    // Optional: append end marker
    fill_buf[len++] = 0x55;

    // Start UART DMA transmit if not already busy
    if (!uart3_busy) {
        uart3_busy = 1;

        // Swap buffer indices: the fill buffer becomes tx buffer
        uart_buf_index_tx   = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;

        HAL_UART_Transmit_DMA(&huart3, fill_buf, len);
    }
    // If busy, the frame will remain in fill buffer until callback swaps it
    return;
}

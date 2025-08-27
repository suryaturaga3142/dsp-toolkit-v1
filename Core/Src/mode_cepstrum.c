/*
 * mode_cepstrum.c
 *
 *  Created on: Aug 26, 2025
 *      Author: surya
 */


#include "main.h"
#include "mode_cepstrum.h"


//volatile uint16_t dac_buf[DAC_BUF_LEN];
static arm_rfft_fast_instance_f32 fft_instance;
static uint8_t uart_buf[2][UART_BUF_LEN];
static float hann_window[FFT_SIZE];

void Cepstrum_Init(void) {

    // --- Generate a test signal with a clear fundamental frequency and harmonics ---
    // This is ideal for testing cepstrum, as it should produce a strong peak.
    // Let's use a 440 Hz fundamental (A4 note)
    float f0 = 440.0f;
    float freqs[4] = {f0, 2*f0, 3*f0, 4*f0}; // Fundamental + 3 harmonics
    float amps[4]  = {1.0f, 0.7f, 0.5f, 0.3f}; // Decreasing amplitudes

    for (int n = 0; n < DAC_BUF_LEN; n++) {
        float value = 0;
        for (int i = 0; i < 4; i++) {
            value += amps[i] * sinf(2.0f * M_PI * freqs[i] * n / SAMPLE_FREQ);
        }
        // Scale and offset for the DAC (0-4095)
        // Max possible amplitude is sum of amps = 2.5
        dac_buf[n] = (uint16_t)((value / 2.5f * 2047.0f) + 2048.0f);
    }

    // --- Initialize Hann window coefficients ---
    for (int i = 0; i < FFT_SIZE; i++) {
         hann_window[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i) / (FFT_SIZE - 1)));
    }

    // --- Initialize the RFFT instance from CMSIS-DSP library ---
    arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

    // --- Start Timers, DAC, and ADC with DMA ---
    HAL_TIM_Base_Start(&htim2);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN);
}


/**
 * @brief Processes ADC data to compute the Cepstrum.
 * @param adc_data Pointer to the raw ADC buffer.
 * @param st_idx Start index of the data to process.
 */
void Cepstrum_procData(volatile uint16_t *adc_data, int st_idx) {

    // --- Buffers for processing stages ---
    float input[FFT_SIZE];          // Input to 1st FFT
    float fft_output[FFT_SIZE];     // Output of 1st FFT (complex, packed)
    float mag_spectrum[NUM_BINS];   // Magnitude spectrum
    float log_mag_spectrum[NUM_BINS]; // Log of magnitude spectrum

    float cepstrum_input[FFT_SIZE]; // Input to 2nd FFT (the log spectrum)
    float cepstrum_output[FFT_SIZE]; // Output of 2nd FFT (the cepstrum)
    float cepstrum_mag[NUM_BINS];   // Magnitude of the cepstrum for display

    static uint16_t cepstrum_u16[NUM_BINS]; // Final scaled data for UART

    // --- Step 1: Convert ADC samples to normalized float and apply Hann window ---
    for (int i = 0; i < FFT_SIZE; i++) {
        float normalized = ((float)(adc_data[st_idx + i]) / 4096.0f) - 0.5f; // Center around 0
        input[i] = normalized * hann_window[i];
    }

    // --- Step 2: Perform the first RFFT on the windowed signal ---
    arm_rfft_fast_f32(&fft_instance, input, fft_output, 0); // 0 for forward FFT

    // --- Step 3: Compute the magnitude of the complex spectrum ---
    arm_cmplx_mag_f32(fft_output, mag_spectrum, NUM_BINS);

    // --- Step 4: Compute the natural logarithm of the magnitude spectrum ---
    // Add a small epsilon to avoid log(0) which is -inf
    for(int i = 0; i < NUM_BINS; i++) {
        mag_spectrum[i] += 1e-9f;
    }
    arm_vlog_f32(mag_spectrum, log_mag_spectrum, NUM_BINS);

    // --- Step 5: Prepare the log spectrum for the second FFT ---
    // The RFFT function requires a full-length real input buffer.
    // We copy our half-length log spectrum and zero-pad the rest.
    arm_copy_f32(log_mag_spectrum, cepstrum_input, NUM_BINS);
    memset(&cepstrum_input[NUM_BINS], 0, (FFT_SIZE - NUM_BINS) * sizeof(float));

    // --- Step 6: Perform a second RFFT on the log-magnitude spectrum ---
    // This transform from the frequency domain back to a time-like domain (quefrency)
    // gives us the cepstrum.
    arm_rfft_fast_f32(&fft_instance, cepstrum_input, cepstrum_output, 0); // Using forward FFT

    // --- Step 7: Calculate the magnitude of the cepstrum for visualization ---
    arm_cmplx_mag_f32(cepstrum_output, cepstrum_mag, NUM_BINS);

    // --- Step 8: Scale cepstrum magnitudes to 16-bit unsigned ints for transmission ---
    // The scaling factor may need tuning based on typical signal levels.
    for (int i = 0; i < NUM_BINS; i++) {
        float scaled = cepstrum_mag[i] * 100.0f;
        if (scaled > 65535.0f) scaled = 65535.0f;
        cepstrum_u16[i] = (uint16_t)scaled;
    }

    // --- Step 9: Format and prepare data for DMA transfer ---
    Cepstrum_printDataDMA(cepstrum_u16);
}

/**
 * @brief Sends the computed cepstrum data over UART using DMA.
 * @param cepstrum_data Pointer to the scaled uint16_t cepstrum data.
 */
void Cepstrum_printDataDMA(uint16_t *cepstrum_data) {
    // Pointer to the current buffer to be filled
    uint8_t *fill_buf = uart_buf[uart_buf_index_fill];
    int len = 0;

    // --- Custom Binary Header ---
    // Use a different start byte to distinguish from RFFT data
    fill_buf[len++] = 0xBB;                                  // Start byte for Cepstrum
    fill_buf[len++] = (uint8_t)(NUM_BINS & 0xFF);             // Number of bins (LSB)
    fill_buf[len++] = (uint8_t)((NUM_BINS >> 8) & 0xFF);      // Number of bins (MSB)
    fill_buf[len++] = (uint8_t)(SAMPLE_FREQ & 0xFF);          // Sampling frequency (LSB)
    fill_buf[len++] = (uint8_t)((SAMPLE_FREQ >> 8) & 0xFF);   // Sampling frequency (MSB)

    // --- Copy raw uint16_t cepstrum data into buffer (little-endian) ---
    for (int i = 0; i < NUM_BINS; i++) {
        fill_buf[len++] = (uint8_t)(cepstrum_data[i] & 0xFF);       // LSB
        fill_buf[len++] = (uint8_t)((cepstrum_data[i] >> 8) & 0xFF); // MSB
    }

    // --- Append end marker ---
    fill_buf[len++] = 0x44;

    // --- Start UART DMA transmit if not already busy ---
    if (!uart3_busy) {
        uart3_busy = 1;

        // Swap buffer indices: the filled buffer becomes the transmission buffer
        uart_buf_index_tx   = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;

        HAL_UART_Transmit_DMA(&huart3, fill_buf, len);
    }
    // If UART is busy, the new frame will wait in the fill buffer.
    // The UART Tx Complete callback will handle sending it next.
    return;
}

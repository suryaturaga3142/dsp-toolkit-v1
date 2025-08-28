/*
 * mode_hilbert.c
 *
 *  Created on: Aug 27, 2025
 *      Author: surya
 */


#include "main.h"
#include "mode_hilbert.h"

static arm_cfft_instance_f32 cfft_instance;
static uint8_t uart_buf[2][UART_BUF_LEN];
static float hann_window[FFT_SIZE];

void Hilbert_Init(void) {

    // --- Generate a linear chirp signal for testing (sweeps from f_start to f_end) ---
    // This is a great test for instantaneous frequency.
    float f_start = 1000.0f; // 1 kHz
    float f_end = 8000.0f;   // 8 kHz
    float sweep_time_s = (float)DAC_BUF_LEN / SAMPLE_FREQ;
    float k = (f_end - f_start) / sweep_time_s; // Chirp rate in Hz/s
    float current_phase = 0.0f;

    for (int n = 0; n < DAC_BUF_LEN; n++) {
        float t = (float)n / SAMPLE_FREQ;
        // Instantaneous frequency at time t
        float inst_freq = f_start + k * t;
        // Increment phase based on instantaneous frequency
        current_phase += 2.0f * M_PI * inst_freq / SAMPLE_FREQ;
        // Generate signal
        float chirp_signal = sinf(current_phase);
        dac_buf[n] = (uint16_t)((chirp_signal * 2047.0f) + 2048.0f);
    }

    // --- Initialize Hann window coefficients ---
    for (int i = 0; i < FFT_SIZE; i++) {
         hann_window[i] = 0.5f * (1.0f - cosf((2.0f * M_PI * i) / (FFT_SIZE - 1)));
    }

    arm_cfft_init_f32(&cfft_instance, FFT_SIZE);

    // --- Start Timers, DAC, and ADC with DMA ---
    HAL_TIM_Base_Start(&htim2);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN);
}

void Hilbert_procData(volatile uint16_t *adc_data, int st_idx) {

    // --- Buffers for processing stages ---
    float input[FFT_SIZE];
    float analytic_signal[FFT_SIZE * 2]; // For complex output of Hilbert
    float amplitude_f32[FFT_SIZE];
    //float phase_f32[FFT_SIZE]; //Uncomment and add some code if all 3 are needed.
    float freq_f32[FFT_SIZE];
    // Final packed data: [amp0, freq0, amp1, freq1, ...]
    static uint16_t packed_output[FFT_SIZE * 2];

    // --- Step 1: Convert ADC samples to normalized float and apply window ---
    for (int i = 0; i < FFT_SIZE; i++) {
        float normalized = ((float)(adc_data[st_idx + i]) / 4096.0f) - 0.5f;
        input[i] = normalized * hann_window[i];
    }

    // --- Step 2: Perform Hilbert Transform to get the analytic signal ---
    myDSP_hilbert_f32(input, analytic_signal);

    // --- Step 3: Calculate Instantaneous Amplitude (Envelope) ---
    arm_cmplx_mag_f32(analytic_signal, amplitude_f32, FFT_SIZE);

    // --- Step 4: Calculate Instantaneous Frequency ---
    // This requires calculating phase, unwrapping it, and differentiating.
    static float last_phase_unwrapped = 0.0f; // Persists between calls

    for (int i = 0; i < FFT_SIZE; i++) {
        // a) Calculate wrapped phase for current sample
        float real = analytic_signal[2 * i];
        float imag = analytic_signal[2 * i + 1];
        float current_phase_wrapped = atan2f(imag, real);

        // b) Unwrap the phase
        float phase_diff = current_phase_wrapped - fmodf(last_phase_unwrapped, 2.0f * M_PI);
        if (phase_diff > M_PI) {
            phase_diff -= 2.0f * M_PI;
        } else if (phase_diff < -M_PI) {
            phase_diff += 2.0f * M_PI;
        }
        float current_phase_unwrapped = last_phase_unwrapped + phase_diff;

        // c) Differentiate unwrapped phase to get frequency
        float freq_rad_per_sample = current_phase_unwrapped - last_phase_unwrapped;
        freq_f32[i] = (freq_rad_per_sample * SAMPLE_FREQ) / (2.0f * M_PI);

        // d) Update state for next iteration
        last_phase_unwrapped = current_phase_unwrapped;
    }

    // --- Step 5: Scale and pack amplitude and frequency into one buffer ---
    for (int i = 0; i < FFT_SIZE; i++) {
        // Scale amplitude (normalized signal has amp ~0.5, scale to uint16 range)
        float scaled_amp = amplitude_f32[i] * 60000.0f;
        if (scaled_amp > 65535.0f) scaled_amp = 65535.0f;
        packed_output[2 * i] = (uint16_t)scaled_amp;

        // Frequency is already in Hz, just cast and clamp
        float freq_val = freq_f32[i];
        if (freq_val < 0.0f) freq_val = 0.0f;
        if (freq_val > 65535.0f) freq_val = 65535.0f;
        packed_output[2 * i + 1] = (uint16_t)freq_val;
    }

    // --- Step 6: Format and prepare data for DMA transfer ---
    Hilbert_printDataDMA(packed_output);
}

/**
 * @brief Sends the packed amplitude/frequency data over UART using DMA.
 * @param packed_data Pointer to the interleaved uint16_t data.
 */
void Hilbert_printDataDMA(uint16_t *packed_data) {
    uint8_t *fill_buf = uart_buf[uart_buf_index_fill];
    int len = 0;

    // --- Custom Binary Header for packed Hilbert Data ---
    fill_buf[len++] = 0xCC;                                  // Start byte
    fill_buf[len++] = (uint8_t)(FFT_SIZE & 0xFF);             // Number of points (LSB)
    fill_buf[len++] = (uint8_t)((FFT_SIZE >> 8) & 0xFF);      // Number of points (MSB)
    fill_buf[len++] = (uint8_t)(SAMPLE_FREQ & 0xFF);          // Sampling frequency (LSB)
    fill_buf[len++] = (uint8_t)((SAMPLE_FREQ >> 8) & 0xFF);   // Sampling frequency (MSB)

    // --- Copy the packed uint16_t data (Amp, Freq, Amp, Freq...) ---
    // We are sending FFT_SIZE * 2 total uint16_t values.
    for (int i = 0; i < FFT_SIZE * 2; i++) {
        fill_buf[len++] = (uint8_t)(packed_data[i] & 0xFF);       // LSB
        fill_buf[len++] = (uint8_t)((packed_data[i] >> 8) & 0xFF); // MSB
    }

    // --- Append end marker ---
    fill_buf[len++] = 0x22;

    // --- Start UART DMA transmit if not already busy ---
    if (!uart3_busy) {
        uart3_busy = 1;
        uart_buf_index_tx   = uart_buf_index_fill;
        uart_buf_index_fill = 1 - uart_buf_index_tx;
        HAL_UART_Transmit_DMA(&huart3, fill_buf, len);
    }
}

/**
 * @brief Manual implementation of the Hilbert Transform using CFFT.
 * @param[in]  input       Points to the real input signal buffer.
 * @param[out] output      Points to the complex analytic signal output buffer.
 * @param[in]  fft_size    Length of the FFT.
 */
void myDSP_hilbert_f32(float32_t* input, float32_t* output)
{
    // CFFT works on complex data, so we need a buffer twice the size of the real input
    // We will use the 'output' buffer as a temporary workspace for the CFFT.

    // 1. Prepare the real input for the Complex FFT
    // Copy real input to the real part of the complex buffer, set imaginary parts to 0.
    for(uint32_t i = 0; i < FFT_SIZE; i++)
    {
        output[2*i] = input[i];     // Real part
        output[2*i + 1] = 0.0f;     // Imaginary part
    }

    // 2. Perform forward Complex FFT
    arm_cfft_f32(&cfft_instance, output, 0, 1); // 0 = forward FFT, 1 = bit reversal

    // 3. Manipulate the spectrum to apply the Hilbert transform
    //    - DC (bin 0) and Nyquist (bin N/2) components are multiplied by 1 (i.e., untouched).
    //    - Positive frequencies (bins 1 to N/2-1) are multiplied by 2.
    //    - Negative frequencies (bins N/2+1 to N-1) are multiplied by 0.

    // Double the positive frequencies
    for(uint32_t i = 1; i < FFT_SIZE / 2; i++)
    {
        output[2*i] *= 2.0f;
        output[2*i + 1] *= 2.0f;
    }

    // Zero out the negative frequencies
    for(uint32_t i = FFT_SIZE / 2 + 1; i < FFT_SIZE; i++)
    {
        output[2*i] = 0.0f;
        output[2*i + 1] = 0.0f;
    }

    // 4. Perform inverse Complex FFT to get the analytic signal
    arm_cfft_f32(&cfft_instance, output, 1, 1); // 1 = inverse FFT, 1 = bit reversal
}

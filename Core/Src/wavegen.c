/*
 * wavegen.c
 *
 *  Created on: Aug 29, 2025
 *      Author: surya
 */


#include "wavegen.h"

void Wavegen_Init(volatile uint16_t *dac_buf) {

#ifdef SIG_SINE_SUM
	float freq[6] = {100, 400, 5000, 9000, 14000, 18000};
	float phase[6] = {0, 0.3, 0.2, 0.1, 0.8};
	for (int n = 0; n < DAC_BUF_LEN; n++) {
		float value = 0;
		for (int i = 0; i < 6; i++) {
			value += (4095 / 2.0f / 6.0f) * (sin( 2.0f * M_PI * ( freq[i] * n / SAMPLE_FREQ + phase[i] ) ) + 1.0f);
		}
		dac_buf[n] = (uint16_t) value;
	}
#endif
#ifdef SIG_HARMONICS
	// This is the signal for testing Cepstrum
	float f0 = 440.0f;
	float freqs[4] = {f0, 2*f0, 3*f0, 4*f0};
	float amps[4]  = {1.0f, 0.7f, 0.5f, 0.3f};
	for (int n = 0; n < DAC_BUF_LEN; n++) {
		float value = 0;
	    for (int i = 0; i < 4; i++) {
	    	value += amps[i] * sinf(2.0f * M_PI * freqs[i] * n / SAMPLE_FREQ);
	    }
	    dac_buf[n] = (uint16_t)((value / 2.5f * 2047.0f) + 2048.0f);
	}
#endif
#ifdef SIG_AMP_MOD
	// This is the signal for testing Hilbert Transform
	float f_carrier = 4000.0f;
	float f_mod = 100.0f;
	float mod_index = 0.7f;
	for (int n = 0; n < DAC_BUF_LEN; n++) {
		float t = (float)n / SAMPLE_FREQ;
		float envelope = 1.0f + mod_index * sinf(2.0f * M_PI * f_mod * t);
		float carrier = sinf(2.0f * M_PI * f_carrier * t);
		float am_signal = envelope * carrier;
		dac_buf[n] = (uint16_t)((am_signal / 1.7f * 2047.0f) + 2048.0f);
	}
#endif
#ifdef SIG_CHIRP
	// --- Generate a linear chirp signal for testing (sweeps from f_start to f_end) ---
	// This is a great test for instantaneous frequency
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
#endif
#ifdef SIG_SQ
	float freq = 500.0f; // 500 Hz square wave (keep it low!)
	uint32_t period_samples = SAMPLE_FREQ / freq;
	for (int n = 0; n < DAC_BUF_LEN; n++) {
		if ((n % period_samples) < (period_samples / 2)) {
			dac_buf[n] = 4095; // High
		}
		else {
			dac_buf[n] = 0;    // Low
		}
	}
#endif


	return;
}

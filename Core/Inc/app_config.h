/*
 * app_config.h
 *
 *  Created on: Aug 14, 2025
 *      Author: surya
 */

#ifndef INC_APP_CONFIG_H_
#define INC_APP_CONFIG_H_

/*
 * Modes of operation:
 * Comment out all the modes except the one used.
 * MODE_RFFT: Simplest, transmits FFT magnitude data only.
 * MODE_CFFT: Alternates between FFT magnitude and phase.
 * MODE_CEPSTRUM: Transmits Cepstral analysis data
 * */

//#define MODE_RFFT
//#define MODE_CFFT
//#define MODE_CEPSTRUM
#define MODE_HILBERT

/*
 * Modes of operation:
 * Comment out all the modes except the one used.
 * MODE_WAVE: Simplest, transmits FFT magnitude data only.
 * MODE_CFFT: Alternates between FFT magnitude and phase.
 * MODE_CEPSTRUM: Transmits Cepstral analysis data
 * */

//#define SIG_SINE_SUM
//#define SIG_HARMONICS
//#define SIG_AMP_MOD
//#define SIG_CHIRP
#define SIG_SQ

#endif /* INC_APP_CONFIG_H_ */

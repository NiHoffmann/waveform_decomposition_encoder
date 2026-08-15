#ifndef FFT_H
#define FFT_H

/* =============================================================================
 * FFT
 * =============================================================================
 *
 * ATTRIBUTION
 * ------------------------------------------------------------------------------
 * The window functions in fft_apply_window() (Hamming, Hann, Triangle,
 * Nuttall, Blackman, Blackman-Nuttall, Blackman-Harris, Flat top, Welch)
 * are ported from the arduinoFFT library's windowing formulas:
 *
 *     https://github.com/kosme/arduinoFFT/tree/master
 *     Copyright (C) 2010 Didier Longueville
 *     Copyright (C) 2014 Enrique Condes
 *     Copyright (C) 2020 Bim Overbohm
 *     Licensed under the GNU General Public License v3 (GPLv3).
 *
 * This derivative work is likewise distributed under GPLv3. It was ported
 * to plain, allocation-free C (no malloc/new, fixed-size arrays only) at a
 * user's request, so it can be used as a small standalone building block
 * outside the Arduino ecosystem. It has NOT been reviewed by the original
 * arduinoFFT authors.
 * ============================================================================= */

#include "../swi_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FFT_WIN_RECTANGLE = 0,  
    FFT_WIN_HAMMING,
    FFT_WIN_HANN,
    FFT_WIN_TRIANGLE,        
    FFT_WIN_NUTTALL,
    FFT_WIN_BLACKMAN,
    FFT_WIN_BLACKMAN_NUTTALL,
    FFT_WIN_BLACKMAN_HARRIS,
    FFT_WIN_FLAT_TOP,
    FFT_WIN_WELCH,
    FFT_WIN_COUNT
} fft_window_t;

/* Applies `window` to data[0..n-1], in place (forward direction only -
 * this project never needs to undo a window). FFT_WIN_RECTANGLE is a
 * no-op, matching arduinoFFT's behavior. */
void fft_apply_window(double *data, int n, fft_window_t window);

/* Direct DFT of data[0..n-1]. Fills amp_out[0..n_bins-1] and
 * phase_out[0..n_bins-1], where n_bins must be n/2 + 1. O(n * n_bins),
 * works for any n (no power-of-two requirement). */
void fft_dft(const double *data, int n, double *amp_out, double *phase_out, int n_bins);

#ifdef __cplusplus
}
#endif

#endif /* FFT_H */

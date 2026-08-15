/* =============================================================================
 * FFT/fft.c
 * See fft.h for the attribution notice (window formulas ported from
 * arduinoFFT: https://github.com/kosme/arduinoFFT). Licensed GPLv3.
 * ============================================================================= */

#include "fft.h"

static inline double fft_sq(double x) { return x * x; }

void fft_apply_window(double *data, int n, fft_window_t window) {
    if (window == FFT_WIN_RECTANGLE) {
        return;
    }

    double samples_minus_one = (double)n - 1.0;

    for (int i = 0; i < (n >> 1); i++) {
        double ratio = (double)i / samples_minus_one;
        double w = 1.0;

        switch (window) {
            case FFT_WIN_HAMMING:
                w = 0.54 - (0.46 * cos(SWI_TWO_PI * ratio));
                break;
            case FFT_WIN_HANN:
                w = 0.50 * (1.0 - cos(SWI_TWO_PI * ratio));
                break;
            case FFT_WIN_TRIANGLE:
                w = 1.0 - ((2.0 * fabs((double)i - (samples_minus_one / 2.0))) /
                           samples_minus_one);
                break;
            case FFT_WIN_NUTTALL:
                w = 0.355768 - (0.487396 * cos(SWI_TWO_PI * ratio)) +
                    (0.144232 * cos(SWI_FOUR_PI * ratio)) -
                    (0.012604 * cos(SWI_SIX_PI * ratio));
                break;
            case FFT_WIN_BLACKMAN:
                w = 0.42323 - (0.49755 * cos(SWI_TWO_PI * ratio)) +
                    (0.07922 * cos(SWI_FOUR_PI * ratio));
                break;
            case FFT_WIN_BLACKMAN_NUTTALL:
                w = 0.3635819 - (0.4891775 * cos(SWI_TWO_PI * ratio)) +
                    (0.1365995 * cos(SWI_FOUR_PI * ratio)) -
                    (0.0106411 * cos(SWI_SIX_PI * ratio));
                break;
            case FFT_WIN_BLACKMAN_HARRIS:
                w = 0.35875 - (0.48829 * cos(SWI_TWO_PI * ratio)) +
                    (0.14128 * cos(SWI_FOUR_PI * ratio)) -
                    (0.01168 * cos(SWI_SIX_PI * ratio));
                break;
            case FFT_WIN_FLAT_TOP:
                w = 0.2810639 - (0.5208972 * cos(SWI_TWO_PI * ratio)) +
                    (0.1980399 * cos(SWI_FOUR_PI * ratio));
                break;
            case FFT_WIN_WELCH:
                w = 1.0 - fft_sq(((double)i - samples_minus_one / 2.0) /
                                  (samples_minus_one / 2.0));
                break;
            default:
                break;
        }

        data[i] *= w;
        data[n - (i + 1)] *= w;
    }
}

void fft_dft(const double *data, int n, double *amp_out, double *phase_out, int n_bins) {
    for (int k = 0; k < n_bins; k++) {
        double re = 0.0, im = 0.0;
        for (int t = 0; t < n; t++) {
            double angle = SWI_TWO_PI * k * t / n;
            re += data[t] * cos(angle);
            im -= data[t] * sin(angle);
        }
        amp_out[k] = swi_sqrt(re * re + im * im) / n;
        phase_out[k] = atan2(im, re);
    }
}

#include "sin_wave_encoder.h"
#include <math.h>

int swe_signal_set(swe_signal_t *sig, const double *values, uint16_t n_samples) {
    if (sig == NULL || values == NULL || n_samples == 0 || n_samples > SWE_MAX_SAMPLES) {
        return 1;
    }
    for (uint16_t i = 0; i < n_samples; i++) {
        sig->values[i] = values[i];
    }
    sig->n_samples = n_samples;
    return 0;
}

int swe_encode(const swe_signal_t *sig, uint16_t n_components, swe_encoded_t *out) {
    if (sig == NULL || out == NULL) {
        return 1;
    }
    int n_samples = sig->n_samples;
    if (n_samples <= 0 || n_samples > SWE_MAX_SAMPLES) {
        return 1;
    }
    int n_bins = n_samples / 2 + 1;
    if (n_components > SWE_MAX_COMPONENTS || n_components > n_bins) {
        return 2;
    }

    static double centered[SWE_MAX_SAMPLES];
    static double amp[SWE_MAX_SAMPLES / 2 + 1];
    static double phase[SWE_MAX_SAMPLES / 2 + 1];
    static uint8_t used[SWE_MAX_SAMPLES / 2 + 1];

    double baseline = 0.0;
    for (int i = 0; i < n_samples; i++) baseline += sig->values[i];
    baseline /= n_samples;
    for (int i = 0; i < n_samples; i++) centered[i] = sig->values[i] - baseline;

    fft_apply_window(centered, n_samples, FFT_WIN_RECTANGLE);
    fft_dft(centered, n_samples, amp, phase, n_bins);

    /* pick the n_components biggest-amplitude bins */
    for (int k = 0; k < n_bins; k++) used[k] = 0;

    double peak_amplitude = 1.0;
    for (int c = 0; c < n_components; c++) {
        int best_k = 0;
        double best_amp = -1.0;
        for (int k = 0; k < n_bins; k++) {
            if (!used[k] && amp[k] > best_amp) {
                best_amp = amp[k];
                best_k = k;
            }
        }
        used[best_k] = 1;
        out->bin_index[c] = (uint8_t)best_k;
        if (c == 0 || best_amp > peak_amplitude) {
            peak_amplitude = best_amp;
        }
    }
    if (n_components == 0) {
        peak_amplitude = 1.0;
    }

    /* quantize amplitude/phase to one byte each */
    for (int c = 0; c < n_components; c++) {
        int k = out->bin_index[c];
        double amp_ratio = (peak_amplitude > 0.0) ? (amp[k] / peak_amplitude) : 0.0;
        double amp_q = round(255.0 * amp_ratio);
        double phase_q = round(255.0 * (phase[k] + SWI_PI) / (2.0 * SWI_PI));
        if (amp_q < 0.0) amp_q = 0.0;
        if (amp_q > 255.0) amp_q = 255.0;
        if (phase_q < 0.0) phase_q = 0.0;
        if (phase_q > 255.0) phase_q = 255.0;
        out->amplitude_q[c] = (uint8_t)amp_q;
        out->phase_q[c] = (uint8_t)phase_q;
    }

    out->n_samples = (uint16_t)n_samples;
    out->n_components = n_components;
    out->baseline = baseline;
    out->peak_amplitude = peak_amplitude;

    return 0;
}

void swe_decode(const swe_encoded_t *enc, double *out_signal) {
    for (int i = 0; i < enc->n_samples; i++) out_signal[i] = enc->baseline;

    for (int c = 0; c < enc->n_components; c++) {
        double a = (enc->amplitude_q[c] / 255.0) * enc->peak_amplitude;
        double p = (enc->phase_q[c] / 255.0) * 2.0 * SWI_PI - SWI_PI;
        double freq = (double)enc->bin_index[c] / enc->n_samples;
        for (int t = 0; t < enc->n_samples; t++) {
            out_signal[t] += 2.0 * a * cos(2.0 * SWI_PI * freq * t + p);
        }
    }
}

double swe_r_squared(const double *reference, const double *reconstructed, uint16_t n) {
    double mean = 0.0;
    for (uint16_t i = 0; i < n; i++) mean += reference[i];
    mean /= n;

    double leftover = 0.0, total_var = 0.0;
    for (uint16_t i = 0; i < n; i++) {
        double d = reference[i] - reconstructed[i];
        leftover += d * d;
        double dv = reference[i] - mean;
        total_var += dv * dv;
    }
    return (total_var > 0.0) ? (1.0 - leftover / total_var) : 1.0;
}

int swe_encode_auto(const swe_signal_t *sig, double target_accuracy,
                     uint16_t max_components, swe_result_t *out) {
    if (sig == NULL || out == NULL) {
        return 1;
    }
    int n_samples = sig->n_samples;
    if (n_samples <= 0 || n_samples > SWE_MAX_SAMPLES) {
        return 1;
    }
    int n_bins = n_samples / 2 + 1;

    uint16_t cap = max_components;
    if (cap > SWE_MAX_COMPONENTS) cap = SWE_MAX_COMPONENTS;
    if (cap > n_bins) cap = (uint16_t)n_bins;
    if (cap < 1) cap = 1;

    static double reconstructed[SWE_MAX_SAMPLES];

    for (uint16_t c = 1; c <= cap; c++) {
        if (swe_encode(sig, c, &out->encoded) != 0) {
            return 2;
        }
        swe_decode(&out->encoded, reconstructed);
        double r2 = swe_r_squared(sig->values, reconstructed, (uint16_t)n_samples);
        if (r2 >= target_accuracy || c == cap) {
            out->accuracy.target = target_accuracy;
            out->accuracy.max_components = max_components;
            out->accuracy.achieved = r2;
            return 0;
        }
    }
    return 3; /* unreachable */
}

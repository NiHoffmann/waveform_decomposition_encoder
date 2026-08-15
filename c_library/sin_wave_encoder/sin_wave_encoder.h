#ifndef SIN_WAVE_ENCODER_H
#define SIN_WAVE_ENCODER_H

/* =============================================================================
 * sin_wave_encoder.h
 * =============================================================================
 *
 * This is the layer built on top of the FFT module (see ../FFT/fft.h for
 * the windowing/DFT primitives and their attribution to arduinoFFT). It
 * adds:
 *   - proper data structures for a signal, its encoded form, and the
 *     accuracy settings/outcome of an encoding (see below)
 *   - the top-N biggest-amplitude-bin selection + byte quantization
 *   - automatic, accuracy-driven component-count selection
 *
 * No dynamic memory allocation anywhere in this module - everything is
 * fixed-size arrays (SWE_MAX_SAMPLES / SWE_MAX_COMPONENTS), overridable at
 * compile time.
 * ============================================================================= */

#include "../swi_defines.h"
#include "../FFT/fft.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- data structures ------------------------------------------------- */

typedef struct {
    double   values[SWE_MAX_SAMPLES];
    uint16_t n_samples;
} swe_signal_t;

/* The encoded form of a chunk: a DC baseline plus up to n_components
 * quantized sine waves (FFT bin index, amplitude byte, phase byte). 
 * baseline is used to quantize and reconstruct.
 */
typedef struct {
    uint16_t n_samples;
    uint16_t n_components;
    double   baseline;
    double   peak_amplitude;
    uint8_t  bin_index[SWE_MAX_COMPONENTS];
    uint8_t  amplitude_q[SWE_MAX_COMPONENTS];
    uint8_t  phase_q[SWE_MAX_COMPONENTS];
} swe_encoded_t;

/* Accuracy settings/outcome for an auto-selected encoding: what was
 * requested (target/cap) vs. what was actually achieved. */
typedef struct {
    double   target;          /* requested R^2 cutoff              */
    uint16_t max_components;  /* requested cap on components        */
    double   achieved;        /* R^2 actually reached               */
} swe_accuracy_t;

/* Bundles the encoded chunk with the accuracy info from swe_encode_auto(). */
typedef struct {
    swe_encoded_t  encoded;
    swe_accuracy_t accuracy;
} swe_result_t;

/* ---- interface ------------------------------------------------- */

/* Encodes "signal" or any other data really. 
 * Static encoding version, use when using same number of compoents for each signal.
 * sig : Signal Array
 * n_components :  Number of Components to use for Encoding.
 * out : Output Array
 */
int swe_encode(const swe_signal_t *sig, uint16_t n_components, swe_encoded_t *out);

/* Same idea, as encode but automantically search for lower possible number of components 
 * for encoding. 
 * sig : Signal Array
 * target_accuracy : Accuracy Cut Off (aka. anything below accuracy is good enough for component search)
 * max_components :  Max Number of Components allowed. Will break off if search failed.
 * out : Output Array
 */
int swe_encode_auto(const swe_signal_t *sig, double target_accuracy,
                     uint16_t max_components, swe_result_t *out);

/* Reconstructs a signal. */
void swe_decode(const swe_encoded_t *enc, double *out_signal);

/* calculate R^2 (Aka. Target Accuracy for swe_encode_auto) coefficient mainly used internally.*/
double swe_r_squared(const double *reference, const double *reconstructed, uint16_t n);

/* Fills a swe_signal_t from a plain array mainly used internally. */
int swe_signal_set(swe_signal_t *sig, const double *values, uint16_t n_samples);

#ifdef __cplusplus
}
#endif

#endif /* SIN_WAVE_ENCODER_H */

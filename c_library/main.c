/* =============================================================================
 * Simple Example class for SWE usage.
 * ============================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sin_wave_encoder/sin_wave_encoder.h"

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr,
                "Usage: %s <target_accuracy> <max_components> <fs> <chunk_index> "
                "<comma_separated_values>\n",
                argv[0]);
        return 1;
    }

    double target_accuracy = atof(argv[1]);
    int max_components = atoi(argv[2]);
    double fs = atof(argv[3]);
    int chunk_index = atoi(argv[4]);

    static double raw_values[SWE_MAX_SAMPLES];
    int n_samples = 0;
    char *token = strtok(argv[5], ",");
    while (token != NULL && n_samples < SWE_MAX_SAMPLES) {
        raw_values[n_samples++] = strtod(token, NULL);
        token = strtok(NULL, ",");
    }

    if (n_samples == 0) {
        fprintf(stderr, "No values parsed from argv[5]\n");
        return 1;
    }

    swe_signal_t signal;
    if (swe_signal_set(&signal, raw_values, (uint16_t)n_samples) != 0) {
        fprintf(stderr, "swe_signal_set failed (check n_samples <= %d)\n", SWE_MAX_SAMPLES);
        return 1;
    }

    swe_result_t result;
    if (swe_encode_auto(&signal, target_accuracy, (uint16_t)max_components, &result) != 0) {
        fprintf(stderr, "swe_encode_auto failed\n");
        return 1;
    }

    static double reconstructed[SWE_MAX_SAMPLES];
    swe_decode(&result.encoded, reconstructed);

    printf("index,time_s,original,reconstructed\n");
    for (int i = 0; i < n_samples; i++) {
        printf("%d,%.6f,%.6f,%.6f\n", i, (double)i / fs, signal.values[i], reconstructed[i]);
    }

    size_t original_bytes = (size_t)n_samples * sizeof(double);
    size_t encoded_bytes = sizeof(float) * 2
                          + (size_t)result.encoded.n_components * (1 + 1 + 1);

    printf("# summary target_accuracy=%.6f max_components=%d n_components=%u r2=%.6f "
           "fs=%.6f chunk_index=%d original_bytes=%zu encoded_bytes=%zu compression=%.6f\n",
           result.accuracy.target, result.accuracy.max_components, result.encoded.n_components,
           result.accuracy.achieved, fs, chunk_index, original_bytes, encoded_bytes,
           (double)original_bytes / (double)encoded_bytes);

    return 0;
}

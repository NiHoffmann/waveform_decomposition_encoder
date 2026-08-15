# ================================
# PPG Sine-Wave Encoder Playground
# ================================

import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def add_noise(signal, noise_std, seed=None):
    """add Gaussian noise to signal"""
    rng = np.random.default_rng(seed)
    return signal + rng.normal(0.0, noise_std, size=signal.shape)


def bandpass_filter(signal, fs, lowcut, highcut, order=4):
    nyquist = 0.5 * fs
    b, a = butter(order, [lowcut / nyquist, highcut / nyquist], btype="band")
    return filtfilt(b, a, signal)


def encode_signal(signal, n_components):
    """FFT the signal, grab the n_components biggest sine waves (by
    amplitude), and pack them down into small quantized integers instead
    of full-precision floats."""
    n_samples = len(signal)
    baseline = np.mean(signal)
    centered_signal = signal - baseline

    spectrum = np.fft.rfft(centered_signal)
    bin_freqs = np.fft.rfftfreq(n_samples)
    amplitudes = np.abs(spectrum) / n_samples
    phases = np.angle(spectrum)

    # biggest amplitude first, take however many components we're allowed
    ranked_bins = np.argsort(amplitudes)[::-1]
    chosen_bins = ranked_bins[:n_components]

    # if there are more than 256 possible frequency bins we need 2 bytes
    # to index into them, otherwise 1 byte is enough
    highest_bin = len(bin_freqs) - 1
    bin_dtype = np.uint16 if highest_bin > 255 else np.uint8
    bin_index_bytes = 2 if bin_dtype == np.uint16 else 1

    # quantize amplitude/phase down to a single byte each
    peak_amplitude = amplitudes[chosen_bins].max() if n_components > 0 else 1.0
    amplitude_q = np.round(255 * amplitudes[chosen_bins] / peak_amplitude).astype(np.uint8)
    phase_q = np.round(255 * (phases[chosen_bins] + np.pi) / (2 * np.pi)).astype(np.uint8)
    bin_index_q = chosen_bins.astype(bin_dtype)

    return {
        "n_samples": n_samples,
        "baseline": baseline,
        "peak_amplitude": peak_amplitude,
        "bin_index": bin_index_q,
        "amplitude_q": amplitude_q,
        "phase_q": phase_q,
        "bin_index_bytes": bin_index_bytes,
        "spectrum_len": len(bin_freqs),
    }


def decode_signal(encoded):
    n_samples = encoded["n_samples"]
    sample_positions = np.arange(n_samples)

    amplitudes = encoded["amplitude_q"].astype(np.float64) / 255.0 * encoded["peak_amplitude"]
    phases = encoded["phase_q"].astype(np.float64) / 255.0 * (2 * np.pi) - np.pi
    freqs = encoded["bin_index"].astype(np.float64) / (2 * (encoded["spectrum_len"] - 1))

    reconstructed = np.full(n_samples, encoded["baseline"], dtype=np.float64)
    for amplitude, freq, phase in zip(amplitudes, freqs, phases):
        reconstructed += 2 * amplitude * np.cos(2 * np.pi * freq * sample_positions + phase)
    return reconstructed


def compute_sizes(signal, encoded, n_components):
    original_bytes = signal.size * 8  # raw float64 samples
    header_bytes = 4 + 4              # baseline + peak_amplitude, stored as float32 each
    bytes_per_component = encoded["bin_index_bytes"] + 1 + 1  # bin index + amp byte + phase byte
    encoded_bytes = header_bytes + n_components * bytes_per_component
    return original_bytes, encoded_bytes


def find_min_components(signal, target_r2, max_components):
    """Figure out the fewest sine-wave components we can get away with
    while still hitting target_r2."""
    n_samples = len(signal)
    search_limit = min(max_components, n_samples // 2)

    baseline = np.mean(signal)
    centered_signal = signal - baseline
    spectrum = np.fft.rfft(centered_signal)
    bin_freqs = np.fft.rfftfreq(n_samples)
    amplitudes = np.abs(spectrum) / n_samples
    phases = np.angle(spectrum)
    ranked_bins = np.argsort(amplitudes)[::-1][:search_limit]

    sample_positions = np.arange(n_samples)
    running_reconstruction = np.full(n_samples, baseline, dtype=np.float64)
    total_variance = np.sum((signal - baseline) ** 2)

    rough_estimate = search_limit
    for count_so_far, bin_idx in enumerate(ranked_bins):
        running_reconstruction += (2 * amplitudes[bin_idx]
                                    * np.cos(2 * np.pi * bin_freqs[bin_idx] * sample_positions
                                             + phases[bin_idx]))
        leftover_error = np.sum((signal - running_reconstruction) ** 2)
        r2 = 1 - leftover_error / total_variance if total_variance > 0 else 1.0
        if r2 >= target_r2:
            rough_estimate = count_so_far + 1
            break

    # now confirm with the actual quantized version, padding upward if needed
    n_components = rough_estimate
    step = max(1, n_components // 10)
    while True:
        encoded = encode_signal(signal, n_components)
        reconstructed = decode_signal(encoded)
        leftover_error = np.sum((signal - reconstructed) ** 2)
        r2 = 1 - leftover_error / total_variance if total_variance > 0 else 1.0
        if r2 >= target_r2 or n_components >= search_limit:
            return n_components, r2
        n_components = min(n_components + step, search_limit)


def process_chunks(values, fs, chunk_size_sec, target_accuracy, max_components,
                    lowcut, highcut, filt_order):
    """Slice the recording into fixed-length chunks and run each one
    through filter -> dynamic-N encode -> decode. Hands back a stats table
    plus the filtered and reconstructed signals stitched back into one
    long array, so we can compare them chunk-by-chunk or as a whole."""
    chunk_samples = int(round(chunk_size_sec * fs))
    n_chunks = len(values) // chunk_samples

    filtered_full = np.zeros(n_chunks * chunk_samples, dtype=np.float64)
    reconstructed_full = np.zeros_like(filtered_full)
    chunk_stats = []

    for chunk_i in range(n_chunks):
        start = chunk_i * chunk_samples
        end = start + chunk_samples
        raw_chunk = values[start:end]

        filtered_chunk = bandpass_filter(raw_chunk, fs=fs, lowcut=lowcut, highcut=highcut,
                                          order=filt_order)
        n_components, r2 = find_min_components(filtered_chunk, target_accuracy, max_components)
        encoded = encode_signal(filtered_chunk, n_components)
        reconstructed_chunk = decode_signal(encoded)
        original_bytes, encoded_bytes = compute_sizes(filtered_chunk, encoded, n_components)

        filtered_full[start:end] = filtered_chunk
        reconstructed_full[start:end] = reconstructed_chunk

        chunk_stats.append(dict(
            chunk=chunk_i,
            n_components=n_components,
            r2=r2,
            original_bytes=original_bytes,
            encoded_bytes=encoded_bytes,
        ))

    stats = pd.DataFrame.from_records(chunk_stats)
    return stats, filtered_full, reconstructed_full, chunk_samples, n_chunks


def plot_example_chunk(values, filtered_full, reconstructed_full, chunk_samples,
                        chunk_index, stats, lowcut, highcut, out_path):
    start = chunk_index * chunk_samples
    end = start + chunk_samples
    raw_chunk = values[start:end]
    filtered_chunk = filtered_full[start:end]
    reconstructed_chunk = reconstructed_full[start:end]

    chunk_row = stats.iloc[chunk_index]
    n_components = int(chunk_row["n_components"])
    r2 = chunk_row["r2"]
    original_bytes = chunk_row["original_bytes"]
    encoded_bytes = chunk_row["encoded_bytes"]
    compression_ratio = original_bytes / encoded_bytes

    fig, (top_ax, bottom_ax) = plt.subplots(2, 1, figsize=(12, 9), sharex=True)

    top_ax.plot(raw_chunk, label="Raw (noisy, unfiltered)", color="#999999",
                linewidth=1.0, alpha=0.8)
    top_ax.plot(filtered_chunk, label=f"Bandpassed ({lowcut}-{highcut} Hz)",
                color="#4C72B0", linewidth=1.4)
    top_ax.set_title(f"Chunk #{chunk_index}: Raw vs. Bandpass-Filtered")
    top_ax.set_ylabel("Amplitude")
    top_ax.legend(loc="upper right")

    bottom_ax.plot(filtered_chunk, label="Bandpassed data", color="#4C72B0", linewidth=1.4)
    bottom_ax.plot(reconstructed_chunk, label=f"Encoded ({n_components} components)",
                   color="#DD8452", linewidth=1.4, linestyle="--")
    bottom_ax.set_title("Bandpassed vs. Sine-Wave Encoded")
    bottom_ax.set_xlabel("Sample index (within chunk)")
    bottom_ax.set_ylabel("Amplitude")
    bottom_ax.legend(loc="upper right")
    bottom_ax.text(
        0.01, 0.02,
        f"R²: {r2:.4f}  |  Compression: {compression_ratio:.1f}x\n"
        f"{original_bytes} bytes -> {encoded_bytes} bytes",
        transform=bottom_ax.transAxes, fontsize=9, va="bottom",
        bbox=dict(boxstyle="round", facecolor="white", alpha=0.8)
    )

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def plot_accuracy_summary(stats, target_accuracy, n_chunks, overall_ratio, out_path):
    chunk_indices = stats.index.values

    fig, (components_ax, r2_ax) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    components_ax.plot(chunk_indices, stats["n_components"], color="#4C72B0", linewidth=0.8)
    components_ax.axhline(stats["n_components"].mean(), color="#DD8452", linestyle="--",
                           linewidth=1, label=f"mean = {stats['n_components'].mean():.1f}")
    components_ax.set_ylabel("Components used (N)")
    components_ax.set_title(f"Dynamic component count per chunk (target R²={target_accuracy})")
    components_ax.legend(loc="upper right")

    r2_ax.plot(chunk_indices, stats["r2"], color="#55A868", linewidth=0.8)
    r2_ax.axhline(target_accuracy, color="#DD8452", linestyle="--", linewidth=1,
                  label=f"target R² = {target_accuracy}")
    r2_ax.set_ylabel("Achieved R²")
    r2_ax.set_xlabel("Chunk index")
    r2_ax.set_title("Reconstruction accuracy per chunk")
    r2_ax.legend(loc="lower right")

    fig.suptitle(f"Whole-dataset encoding summary - {overall_ratio:.1f}x overall compression, "
                 f"{n_chunks} chunks", y=1.0)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def plot_whole_file_overlay(filtered_full, reconstructed_full, fs, n_chunks,
                             overall_ratio, out_path):
    n_samples = len(filtered_full)
    step = max(1, n_samples // 20000)
    sample_idx = np.arange(0, n_samples, step)
    time_minutes = sample_idx / fs / 60.0

    leftover_error = np.sum((filtered_full - reconstructed_full) ** 2)
    total_variance = np.sum((filtered_full - np.mean(filtered_full)) ** 2)
    overall_r2 = 1 - leftover_error / total_variance

    fig, ax = plt.subplots(figsize=(14, 5))
    ax.plot(time_minutes, filtered_full[sample_idx], label="Real (bandpassed) signal",
            color="#4C72B0", linewidth=0.6)
    ax.plot(time_minutes, reconstructed_full[sample_idx], label="Reconstructed (stitched from chunks)",
            color="#DD8452", linewidth=0.6, linestyle="--", alpha=0.85)
    ax.set_xlabel("Time (min)")
    ax.set_ylabel("Amplitude")
    ax.set_title("Whole-File Overlay: Real vs. Reconstructed")
    ax.legend(loc="upper right")
    ax.text(
        0.01, 0.02,
        f"Overall R²: {overall_r2:.4f}  |  Compression: {overall_ratio:.1f}x",
        transform=ax.transAxes, fontsize=9, va="bottom",
        bbox=dict(boxstyle="round", facecolor="white", alpha=0.8)
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main():
    ##### SETTINGS ######
    csv_path = "../Dataset/P01/E4/BVP.csv"
    fs = 64.0            # sampling rate from data set description

    chunk_size_sec = 16.0
    target_accuracy = 0.98  
    max_components = 200    
    example_chunk_index = 115

    ##### FILTER_VALUES ####
    lowcut = 1.0
    highcut = 4.0
    filt_order = 4

    #### ARTIFICIAL_NOISE ####
    noise_std = 10.0
    noise_seed = 100

    example_plot_path = "ppg_example_chunk.png"
    summary_plot_path = "ppg_accuracy_summary.png"
    overlay_plot_path = "ppg_whole_file_overlay.png"

    # load file and add noise
    values = pd.read_csv(csv_path)["value"].values.astype(np.float64)
    if noise_std > 0:
        values = add_noise(values, noise_std, seed=noise_seed)

    # filter and encode every chunk
    stats, filtered_full, reconstructed_full, chunk_samples, n_chunks = process_chunks(
        values, fs, chunk_size_sec, target_accuracy, max_components,
        lowcut, highcut, filt_order
    )
    example_chunk_index = max(0, min(example_chunk_index, n_chunks - 1))
    overall_ratio = stats["original_bytes"].sum() / stats["encoded_bytes"].sum()

    # plot close up of selected chunk
    plot_example_chunk(values, filtered_full, reconstructed_full, chunk_samples,
                        example_chunk_index, stats, lowcut, highcut, example_plot_path)

    # accuracy/component count across every chunk
    plot_accuracy_summary(stats, target_accuracy, n_chunks, overall_ratio, summary_plot_path)

    # the whole recording, real vs. reconstructed
    plot_whole_file_overlay(filtered_full, reconstructed_full, fs, n_chunks,
                             overall_ratio, overlay_plot_path)


if __name__ == "__main__":
    main()

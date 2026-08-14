# Sine-Wave Decomposition Compression - Playground

A small experiment in lossy compression using sine-wave (Fourier) decomposition,
with PPG (photoplethysmogram / heartbeat) sensor data as the test subject.

## The basic idea

Almost any repeating or quasi-repeating signal - a heartbeat, a song,
an engine's vibration, .... - can be described as a sum of sine waves of different
frequencies, amplitudes, and phases. Aka. A Fourier series. The useful
trick for compression is that **most of a signal's "shape" usually comes from
just a handful of the biggest sine waves**, not all of them.

So instead of storing every raw sample, we can:

1. Run an FFT on a chunk of the signal to find its sine-wave components.
2. Keep only the *N* biggest ones (by amplitude) - the ones that matter most.
3. Store just those N components (frequency, amplitude, phase) instead of
   every sample.
4. To play it back, just add those N sine waves together again.

What we get is a Lossy Compression.: throwing away the small, less important components
loses some detail, but keeping the most important components.

Applied here to one 16-second chunk of real PPG data: noisy raw signal filtered down to the heartbeat band, then rebuilt from the sin wave decomposition:

![Example chunk: raw vs. filtered, filtered vs. reconstructed](images/ppg_example_chunk.png)

## Why PPG data works well as an example

PPG is a good showcase signal because it's **quasi-periodic** - one heartbeat
looks a lot like the next one. That means a small number of sine waves (the
fundamental heart-rate frequency plus a few harmonics) can already captures most
of the waveform's shape.

Its important to note that the less periodic and
more random a signal is, the fewer sine waves will help, and the worse this
approach does. This trick doesn't work on genuinely random noise, since noise
has no repeating structure for a handful of sine waves to capture.

## Why we chunk the signal instead of encoding it all at once

A real recording isn't perfectly stationary - heart rate drifts, someone
starts walking, a bit of motion noise shows up. If you try to fit *one* set
of sine waves across an entire minutes long recording, you will end up with a 
very bad approximation of the actual wave form.

The fix is simple: **cut the recording into short chunks** (16 seconds here)
and re-run the encoder independently on each one. Every chunk gets its own
freshly fitted set of sine waves, so local changes in behavior (a burst of
motion, a shift in heart rate) only affect that chunk's component count, not
the whole file. It also means noisier/busier chunks can automatically use
more components while calm chunks use fewer, which keeps overall accuracy
consistent instead of being dragged down by the hardest part of the
recording. You can see that adaptivity directly below - component count
spikes exactly where the signal gets harder, while accuracy stays pinned
near the target across all chunks:

![Per-chunk component count and accuracy across the whole recording](images/ppg_accuracy_summary.png)

Stitching every chunk's reconstruction back together and laying it over the
real signal shows the same story at the whole-file level:

![Whole file: real signal vs. stitched-together reconstruction](images/ppg_whole_file_overlay.png)

On the ~62-minute test recording used here, this reaches roughly **69x
compression** while keeping every chunk at 98%+ reconstruction accuracy.

## Python playground overview

The Python side is where the approach gets tested and tuned - it's meant for
quickly trying out ideas.

What it does:
- Loads a chunk of PPG data, optionally adds synthetic noise to stress-test.
- Bandpass-filters each chunk to the physiological heartbeat band, dropping
  baseline drift and high-frequency noise before compression even starts.
- Dynamically picks the smallest number of sine-wave components needed to
  hit a target reconstruction accuracy (Aka. R²) per chunk.
- Quantizes those components down to a few bytes each.
- Produces the plots above: a close-up of one example chunk, accuracy/
  compression stats across every chunk, and a whole-file overlay of the real
  vs. reconstructed signal.

This is the sandbox for answering questions like "how few components can we
get away with", "how does chunk length affect accuracy", or "how does this
hold up under noise" - before committing to a fixed, efficient
implementation (or just to play around with it).

## C-library overview

TBD

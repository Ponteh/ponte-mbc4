# Ponte MBC4 DSP model 2 — black-box fit

Date: 2026-08-18

The offline analyzer in `BlackBoxAnalysis.cpp` was run against the 48 kHz
MC2000 test pack and all 49 corresponding original plug-in renders. The input
and render WAV files remain external research material and are not distributed
with the product.

## Identified blocks

- Crossover: two cascaded Butterworth second-order sections per LR4 branch.
  The measured two-band impulse error is approximately `9.1e-9`; no crossover
  saturation was measurable.
- Stereo detector: maximum absolute sample across channels.
- Hard knee: standard peak-domain threshold/ratio law.
- Negative and positive Knee: monotone Hermite LUTs derived from the complete
  T06/T07 level series at `T=-24 dB`, `R=4:1`; ratio interpolation uses the
  normalized compression slope.
- Attack: one-pole peak attack with the fitted displayed-time mapping
  `tau = attack * 0.51 / (1 + attack / 800)`.
- Type-1: measured stretched-exponential release, including the pure-peak rule
  that ignores a second event below the release envelope.
- Type-2: stretched release scale `0.838` and detector-envelope adaptation
  `1 - 0.58 * D/E`. T10 error is below `0.02 dB` at second-event onset and
  below `0.11 dB` at its end for -20, -15 and -10 dBFS events.
- BITE: fast/slow onset classifier, fitted T13 transfer curve, and separate
  18 ms relief memory. Measured/model maximum relief in 5 ms windows:

  | Rise | Original | Model | Absolute error |
  |---:|---:|---:|---:|
  | 0 ms | 2.862 dB | 2.848 dB | 0.014 dB |
  | 10 ms | 1.183 dB | 1.118 dB | 0.065 dB |
  | 100 ms | 0.322 dB | 0.338 dB | 0.016 dB |

## Remaining empirical limits

- The supplied pack contains no Auto renders. Auto therefore remains the
  documented crest-factor, peak/RMS program-dependent fallback; manual Attack
  and Release are ignored in this mode.
- Separate MC404 Band 2 and Band 3 impulse renders are not present. The
  four-band implementation uses a compensated LR4 tree with unity-magnitude
  recombination, but its internal-band phase cannot yet be claimed identical.
- Only 48 kHz original renders were supplied. Coefficients and timing are
  sample-rate invariant by construction and regression-tested at 44.1, 48 and
  96 kHz, but original plug-in sample-rate dependence remains unmeasured.

These limits require new reference renders, not additional guessing in the
production DSP.

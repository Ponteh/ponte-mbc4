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

- The complete Auto parameter mapping is defined by the relevant
  `TEST_SPEC.md`; filename suffixes only identify the manual variant.  All 20
  MC404 T02 `min`/`max` pairs and all 9 MC404 T08 pairs are sample-identical,
  so those series ignore manual Attack/Release.  T08 MC303 pulse-series pairs
  differ materially, and T11 Auto/BITE renders sustain gain reduction on a
  band-isolated burst.  The available files therefore do not support a single
  global “Auto ignores manual controls” or “Auto bypasses compression” rule.
  Auto remains a conservative crest-factor, peak/RMS fallback until a
  segment-aware fit reconciles the MC303, MC404 and BITE trajectories.
- The additional suite supplies separate MC404 Band 2 and Band 3 impulse
  renders. They agree with the compensated LR4 tree to numerical precision for
  the supplied standard, close and extreme crossover configurations.
- Only 48 kHz original renders were supplied. Coefficients and timing are
  sample-rate invariant by construction and regression-tested at 44.1, 48 and
  96 kHz, but original plug-in sample-rate dependence remains unmeasured.
- The final sidechain folder supplies Program/Key stimuli but no paired
  reference render.  The production plug-in now exposes a conventional
  band-split external detector; its exact relationship to the original
  plug-in still requires those reference renders.

These limits require new reference renders, not additional guessing in the
production DSP.

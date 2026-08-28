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

- The additional pack includes Auto `min`/`max` renders, and the two variants
  are sample-identical.  The complete parameter mapping is defined by the
  folder's `TEST_SPEC.md`; the filename suffix only identifies the manual
  Attack/Release position.  This confirms that manual Attack/Release are
  ignored.  A dedicated segment-aware extraction pass is still required to
  fit the Auto gain trajectory from these existing renders, rather than
  inferring configuration from filename text.  Auto remains the documented
  crest-factor, peak/RMS program-dependent fallback until that fit is complete.
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

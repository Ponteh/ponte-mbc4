# MC2000 extended test-pack audit

Date: 2026-08-28

## Scope read

The specifications and manifests in all three supplied packs were read before
changing DSP behaviour:

- `MC2000_BlackBox_TestPack_48k` — 76 WAV files;
- `MC2000_Additional_BlackBox_Tests` — 1,101 WAV files;
- `MC2000_Integration_Final_DSP_TestPack` — 396 WAV files.

All 1,573 WAV headers validate as stereo IEEE-float, 32-bit WAV. The baseline
and most extended tests are 48 kHz; the sample-rate suites also contain the
declared 44.1, 88.2, 96 and 192 kHz stimuli.

## Repeatable findings

- The MC303/MC404 rendered crossover impulse files agree with the compensated
  LR4 tree to numerical precision for the standard configurations. The
  closest-spaced and extreme configurations also remain at or below -134 dB
  NRMSE.
  No change to the crossover topology is justified.
- The additional Auto `min`/`max` pairs are sample-identical. Manual Attack and
  Release are therefore ignored in that mode, as required by the test.
- The `.md` specification, rather than the abbreviated filename, defines the
  complete Auto configuration. The preliminary fixed-window gain check is not
  sufficient to extract an Auto curve from the supplied material; it must be
  replaced by a segment-aware analysis that uses the specified settings and
  event timings. No conclusion about a neutral/bypassed Auto mode is valid.
- The final sidechain folder contains Program and Key sources, but no rendered
  output pair. It supports testing a correct sidechain implementation but not
  numerical matching to the original.

## Behaviour implemented from the uncovered integration scope

- The VST3 now exposes an optional mono/stereo sidechain bus. Its audio is
  split by the same crossover as the program and replaces program detection on
  a per-band basis; it never enters the audible program path.
- Crossover targets are smoothed sample-by-sample and filter coefficients are
  refreshed every 16 samples, avoiding buffer-size-dependent parameter jumps.
- Solo is now a 5 ms crossfade instead of an instantaneous mute, preserving
  automation continuity.

The included DSP tests cover external-key gain reduction and Solo transition
behaviour. Exact Auto and original-sidechain fits remain deliberately marked
as pending controlled reference renders.

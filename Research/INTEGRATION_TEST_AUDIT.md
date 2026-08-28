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
- The `.md` specification, rather than the abbreviated filename, defines the
  complete configuration. T02 has 20 complete MC404 Auto min/max pairs, all
  sample-identical (the two finite numerical floors are below -444 dB). T08
  adds 9 MC404 pairs, again sample-identical. In contrast, the MC303 T08
  PulseTrain 1/10/50 Hz pairs differ by 10.4–14.2 dB NRMSE and must not be
  collapsed into an MC404-only rule.
- Segment-aware traces show MC404 T08 and T11 Auto reaching approximately
  -15.34 dB on in-band events, while the T02 continuous-sine series settles
  near its neutral path. This establishes that a global Auto-bypass change
  would be wrong; the current crest-factor fallback remains deliberately
  conservative pending a cross-model trajectory fit.
- T03 supplies 248 source/render pairs covering Attack, Knee, Release and
  Type-2; their documented parameter mapping corresponds to the existing
  per-band gain computer and ballistics paths. T04 supplies 60 BITE triplets;
  BITE 1 and 5 are byte-identical there, whereas T11 retains small
  internal-band differences. This is not enough to replace the established
  BITE fit with a single threshold shortcut.
- T06 supplies 120 single-tone source/render pairs plus IMD material. In
  one-second steady measurements, the fundamental gain changes by the exact
  requested Input Gain and is invariant across the tested -30 to -1 dBFS
  source levels. The frequency-dependent residual harmonic level is likewise
  invariant. No level/headroom breakpoint is measurable, so a new clipper or
  saturator is explicitly not added on the basis of this pack.
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
behaviour. They now also cover Type-2 and Auto external-key processing and
all T12 source rates (44.1/48/88.2/96/192 kHz). Exact Auto and original-
sidechain fits remain deliberately marked as pending controlled reference
renders.

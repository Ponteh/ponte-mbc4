# R1 linear-control release - 2026-09-23

## Scope and decision

Implement R1 in local 0.2.3 as DSP_MODEL_7. This is not a published release.
The model is derived from the original measurements recorded on September 20,
not from a new fit to the validation outputs. Baseline production source is
`5d8bd92709b113a7957db591a609ef3044dd350e` / DSP_MODEL_6; the September 20
research snapshot is committed separately as `4280b54`.

All **8 R1 conditions improve in both mean and p95 attenuation error**.
The other **53 conditions are sample-for-sample identical** to model 6.
All 12 isolated release segments improve in GR trajectory RMSE.
The 61 comparisons include diagnostic acquisitions with known setting issues;
they are not 61 original-plugin equivalence passes.

## Implemented law

For `s = 1 - 1/ratio`, release in seconds and initial reduction `g0`:

```text
g(t) = 20*s*log10(1 + (10^(g0/(20*s)) - 1)*exp(-t/Release))
```

The implementation advances from current GR using a stable log-domain
increment (`expm1` / `log1p`), avoiding positive exponential overflow near
ratio 1. Release coefficient calculation is cached. Automating Release or
ratio above unity changes the subsequent rate without recomputing the curve
from its old starting point. Exactly unity ratio clears the release state;
this is a bypass boundary, not a guaranteed click-free ratio-to-unity ramp.
Prepare invalidates the coefficient cache, reset clears envelopes and GR.

Manual attack and the lower-event rule are retained. R2, Auto, BITE,
crossover, routing and GUI ballistics retain their algorithms. Parameter IDs
and state schema are unchanged; the DSP model marker advances to 7. Existing
R1 presets intentionally change sound; there is no legacy-model selector.

Direct original evidence is limited to ratio 2:1 / knee 0 / BITE 1. The
ratio-normalised extension is an explicit extrapolation. Numerical stability
at other ratios/knees is not evidence of exact McDSP behaviour there.

## Full-engine comparison

MAE/p95 use 10 ms stereo RMS, neutral level above -65 dBFS in both chains,
each engine divided by its own neutral. No time alignment, gain-offset
correction or new parameter fitting is applied. Sources and baseline renders
are hash checked. Exact `Txxx.wav` originals retain the September 20 selection;
silent, missing and format-incompatible cases stay excluded.

| Case | MAE model 6 | MAE model 7 | p95 model 6 | p95 model 7 |
| --- | ---: | ---: | ---: | ---: |
| T003, 315 Hz R250 | 0.0771 | 0.0319 | 0.3182 | 0.1257 |
| T007, lower-event probe | 0.0676 | 0.0280 | 0.3561 | 0.1258 |
| T011, 2 kHz R250 | 0.0832 | 0.0286 | 0.3230 | 0.1175 |
| T048, stereo probe | 0.0705 | 0.0364 | 0.2183 | 0.1275 |
| T083, harmonic probe R250 | 0.1552 | 0.0604 | 0.5668 | 0.1823 |
| T084, harmonic probe R500 | 0.2098 | 0.0591 | 0.5966 | 0.1794 |
| T085, 315 Hz R500 | 0.1023 | 0.0268 | 0.4084 | 0.0832 |
| T086, 2 kHz R500 | 0.1055 | 0.0193 | 0.4192 | 0.0573 |

Errors are in dB. T083/T084 are synthetic independent probes, not real voice.
The 53 unchanged conditions include neutral, Auto, R2 and the Auto-based BITE
acquisitions. This protects those paths against unintended changes; it does
not fix their previously documented mismatches with original recordings.

For the drop at 22 seconds, signed least-squares gain on the isolated carrier
uses 1 ms windows. Times below are relative to that drop, at window centres.

| Case | Original t50 | Model 6 t50 | Model 7 t50 | Original t10 | Model 7 t10 |
| --- | ---: | ---: | ---: | ---: | ---: |
| T003 | 364.5 | 334.5 | 367.5 | 899.5 | 903.5 |
| T011 | 365.5 | 335.5 | 367.5 | 901.5 | 905.5 |
| T085 | 732.5 | 668.5 | 735.5 | 1805.5 | 1810.5 |
| T086 | 735.5 | 669.5 | 737.5 | 1809.5 | 1813.5 |

Times are milliseconds. Across drops at 22, 33 and 43 seconds, release GR
RMSE improves from 0.198--0.413 dB to 0.019--0.059 dB (25 ms after each drop
through 3 seconds, original GR above 0.03 dB). Residual detector/plateau and
full-chain differences remain; no artificial meter offset is introduced.

Exact values: [comparison](comparison.json), [release segments](release_segments.json).
Hashes and unchanged-case IDs: [verification](verification.json).

## Reproduction and tests

Windows x64, MSVC 19.40.33813.0, CMake 3.30.1, local JUCE 9.0.1.
Separate build: `build/MC2000-r1-2026-09-23`, keeping the model-6 renderer in
`build/MC2000-analysis-vs` and its output arrays untouched. Final Release
flags match that baseline: `/DWIN32 /D_WINDOWS /EHsc`, `/O2 /Ob2 /DNDEBUG`.
The first sandbox-limited configure left incomplete flags; its temporary
renders were discarded by regeneration, not used for the results above.

From the workspace root:

```powershell
cmake -S products/MC2000 -B build/MC2000-r1-2026-09-23 -G "Visual Studio 17 2022" -A x64
cmake --build build/MC2000-r1-2026-09-23 --config Release --target MC2000OriginalPackRender MC2000Tests MC2000UITests -j 4
ctest --test-dir build/MC2000-r1-2026-09-23 -C Release --output-on-failure
& build/mc2000-meter-analysis-tools/python/python.exe products/MC2000/Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/compare_r1_2026_09_23.py --render
```

The portable Python already contains numpy/scipy. A normal Python with those
packages can also run the script. Without `--render`, it rechecks hashes and
remeasures existing output arrays. WAVs, raw arrays and binaries remain local;
JSON summaries are committed. Historical September 20 scripts intentionally
check model-6 source hashes; use that frozen source when reproducing their
original audit, not the current model-7 tree.

New numerical tests cover 44.1/48/88.2/96/192 kHz, Release 25/250/500/2500 ms,
ratios 1.01/2/4/10, knees -10/0/15, unchanged attack, closed-form trajectory,
monotonicity, finite extreme near-unity state, unity clearing, reset,
Release/ratio changes, mode transitions and 32/512/irregular host blocks.
Existing routing, sidechain, BITE, Auto and GUI tests remain in the suite.

Final Windows Release CTest: **2/2 PASS**, DSP 4.42 s, GUI/state 6.58 s,
11.04 s total. Before modification the frozen build also passed 2/2
(7.55 s total). These test durations are not a controlled CPU benchmark.
Final review also verified every recorded Source hash against the worktree
and checked that all eight R1 cases improve both MAE and p95.

## Still open

- Real-voice listening/DAW comparison and native macOS/Linux tests of model 7.
- Original ratio/knee matrix, audible automation and sidechain acquisitions.
- BITE amplitude/map/duration recalibration; no BITE fix in this change.
- Updated CPU/tail benchmark, full DSP nap and oversampling GO/NO-GO.
- GUI acquisition uncertainties, GR video validation and final release gate.

No full fidelity, CPU improvement or release-readiness claim is made.

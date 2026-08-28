# Next DSP Validation Pack Audit

## Scope and reproducibility

This audit covers the renders added to `MC2000_Next_DSP_Validation_Pack`.
The `TEST_SPEC.md` in each category is the authority for parameter settings;
render prefixes only identify a model, control variant, or solo state.

Build and run the analyser with:

```powershell
cmake --build C:/build/PonteMBC4 --config Release --target MC2000NextPackAnalysis
C:/build/PonteMBC4/MC2000NextPackAnalysis_artefacts/Release/MC2000NextPackAnalysis.exe `
  C:/Users/ricca_000/Downloads/MC2000_Next_DSP_Validation_Pack auto
```

The optional final argument is `auto`, `bite`, `stereo`, or `sample`.

## Observations used

### Auto and crossover

- The 39 manual min/max pairs (13 sources for each of MC202, MC303, and
  MC404) null exactly or below the analyser's numerical floor. Auto is
  therefore independent of the exposed Attack and Release controls.
- This result is consistent across step, continuous, pulse-train,
  crest-factor, and noise-like source material. It confirms the existing
  `Ballistics` API contract: manual timing arguments do not affect
  `TCMode::automatic`.
- The data establishes that property, not a unique Auto timing law. The
  crest-factor law remains a clearly labelled fallback until a direct
  segment-by-segment model/reference fit is available.

### BITE

For the `All` renders, the BITE response is a brief output-level relief rather
than a static gain change. Averaged over the three specified events:

| Comparison | 5 ms | 10 ms | 25 ms | 50 ms | 95 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| BITE 5 minus BITE 1 | +0.127 dB | +0.087 dB | +0.028 dB | +0.015 dB | +0.008 dB |
| BITE 10 minus BITE 1 | +0.538 dB | +0.416 dB | +0.120 dB | +0.063 dB | +0.031 dB |

This supports the existing fast/slow envelope relief topology and its neutral
steady state. It does not justify changing its constants globally.

The Solo passes do not yield one monotonic cross-band BITE curve: some low and
mid-band BITE-5 observations are materially larger than their BITE-10
counterparts, while the high and upper-mid observations remain close to the
`All` result. This may be a real band-dependent interaction, but it conflicts
with a single global control law. It is recorded as an unresolved fit rather
than used to overfit the production DSP.

With BITE fixed to 5 and `All` selected, Type-2 differs only slightly from
Type-1 over the measured event windows (about -0.007 to -0.132 dB), while Auto
has greater attenuation early in the event (about -2.20 dB at 5 ms, reducing
to -0.50 dB at 95 ms). These are reference characteristics to use in a later
Auto trajectory fit.

### Stereo detector

For all four stereo source variants, including anti-phase, the measured left
and right steady-state gain deltas are 0 dB for both `All` and Solo Band 2.
That validates the existing per-band maximum-absolute-value stereo detector:
it links gain reduction without cancelling anti-phase input.

### Sample-rate renders

The sample-rate data cannot yet validate the full 44.1/48/88.2/96/192 kHz
matrix from its specification:

- no `All` renders are present for 192 kHz;
- the nine `sololow` files named as 88.2, 96, or 192 kHz are physically 48 kHz
  WAVs, while their corresponding source files are at their named native rate.

The analyser reports these mismatches and excludes them from amplitude-ratio
measurements. The DSP's sample-rate regression tests remain in place, but no
reference-fit change is derived from the affected files.

### Not rendered yet

`03_AUTOMATION` and `04_SIDECHAIN` contain their specified sources but no
reference renders. The existing smoothing and external-key implementation is
therefore retained and tested functionally, not fitted to the original.

## Implementation decision

No production DSP coefficient was changed by this audit. The new renders
validate the current absolute stereo link and Auto-manual-control separation;
the remaining BITE Solo and sample-rate inconsistencies do not support a safe
global retune. `MC2000NextPackAnalysis` is added so future clean render sets
can be evaluated with the same associations, including the historical
`solomdi`/`solomid` label correction and Windows `(2)` duplicate suffix.

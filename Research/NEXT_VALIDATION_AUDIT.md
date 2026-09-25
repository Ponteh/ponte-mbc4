# Next DSP Validation Pack Audit

**September 19 follow-up:** a separate controlled 60-second acquisition pack
now supports replacing the Auto crest-factor fallback with the DSP_MODEL_6
linear-control release model. This historical audit and its incompatible
datasets remain unchanged as evidence. See the [new audit and limitations](NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-19/REPORT.md).

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
steady state.

The Solo passes do not yield one monotonic cross-band BITE curve: some low and
mid-band BITE-5 observations are materially larger than their BITE-10
counterparts, while the high and upper-mid observations remain close to the
`All` result. This may be a real band-dependent interaction, but it conflicts
with a single global control law. It is recorded as an unresolved fit rather
than used to overfit the production DSP.

### Low/Mid retest

The later Studio One Low/Mid retest resolves the BITE-5 ambiguity.  For the
carrier that belongs to the selected Solo band, the measured BITE-5 relief is
`+0.218 dB` (Low) and `+0.177 dB` (Mid) at 5 ms; BITE-10 is `+0.829 dB` and
`+0.808 dB`.  The new BITE-10 stems null the historical BITE-10 renders,
whereas several historical BITE-5 stems do not.  The latter are therefore not
used as calibration anchors.

The production BITE control map is updated accordingly: it uses smooth anchors
of 0 at BITE 1, 0.062 at BITE 5, 0.256 at BITE 10, and 1 at BITE 50 while
retaining the established 3.2 dB maximum transient-relief ceiling.  This gives
the required nonlinear low-range control response without changing the
fast/slow detector, its decay, or steady-state neutrality.

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

The BITE control map was changed after the Low/Mid retest, while Auto and
stereo retain their validated implementation. `MC2000NextPackAnalysis` is
added so future clean render sets can be evaluated with the same associations,
including the historical `solomdi`/`solomid` label correction and Windows `(2)`
duplicate suffix.

## September 24 follow-up

The controlled pack now includes corrected model/rate cases, native 96/192
kHz renders (DAW rate confirmed, buffer 512), and the dry MP3 for two R1
voice exports. Model 9 improves Auto noise/onsets; T045 remains diagnostic.
Original external-sidechain acquisition is unavailable in this setup.
This historical audit is retained; see the
[current acquisition report](NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-24/REPORT.md)
and [nap/CPU/oversampling implementation](NAP_CPU_AUTO_OVERSAMPLING_2026-09-24.md).

## Verifica tecnica locale conclusiva - 24 settembre 2026

Matrice wrapper/DSP e audit callback, stress concorrente, profilo GUI e
correzione workflow CI completati; suite locale 4/4 PASS. Nessuna nuova
legge audio o compensazione del meter. Restano host reale, T045 e confronto
visuale con l'originale. [Risultati e limiti](technical_validation_2026-09-24/REPORT.md);
[prove mirate dell'utente](VALIDAZIONE_UTENTE_0.2.3.md).

## Consegna del 25 settembre 2026

T044/T045 rifatti riducono la grande anomalia precedente (MAE attiva circa
0,29-0,30 dB); WAV 192 kHz, frequenza nativa di questa coppia da confermare.
Voci SOLO 2/3 confrontate: circa 0,19 dB di scarto medio, senza neutro
originale/dry WAV. Sei video 60 fps permettono la verifica relativa dei
meter: mantenuto il modello dei livelli, GR grafica ricalibrata 45/90 ms.
Nessuna modifica audio. Video muti e meter McDSP IN selezionato limitano
la latenza assoluta e la verifica OUT per banda. Punto 1 host ancora atteso.
[Rapporto aggiornato](NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-25/REPORT.md).

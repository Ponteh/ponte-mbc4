# Sidechain spectrum - 2026-10-08

Local Windows x64 candidate for Ponte MBC4 0.2.5. State schema 4 and DSP model 11
are unchanged. This extends the per-band NO/ALL standard sidechain selectors.

## Display

The original program spectrum remains grey. The external key spectrum uses the
existing lime accent at 35% opacity, with the same thin stroke and level scale.
Only enabled ALL bands show the key. Nonadjacent selections produce separate
path segments; NO regions never show or bridge the overlay. SOLO changes output
monitoring, not this input display. Cropping is visual: detectors still receive
the complete actual filter response, including its tails.

Both IIR and Linear Phase use the active crossover response. The magnitude is
weighted by the complex sum of selected band responses (FIR bands share phase).
These are input spectra before dynamics; the new trace is neither GR nor output.
Standard sends arrive as one audio mix. Independent named sources remain
unavailable in this build; no companion sender has been introduced.

## Reuse and cost

Source/UI/StereoSpectrum.h provides the same Hann-window analysis and elapsed-time
level ballistics for both signals. One FFT plan, window and workspace are shared
on the message thread. Mono/identical L/R windows use one transform; stereo
uses at most two and combines channel magnitudes without L/R cancellation.
The 2048-sample window has 50% overlap. Four ALL bands reuse one key analysis.

The existing bounded FIFO carries time-aligned program L/R, key L/R and a captured
routing mask. Its storage and the GUI drain buffer are allocated once; callbacks
never resize them or run FFTs. There is no FIFO production when the GUI is closed.
NO/disabled bands skip key capture and analysis; hidden GUI skips spectrum updates.
Exactly silent windows skip FFTs. Filter weights are cached and reused until
the active response, sample rate, band count, IN or routing changes.

FIFO overflow discards stale audio. Routing changes discard old key windows;
an unavailable key removes the overlay without substituting program audio.
The original audio path and latency are unchanged by this display feature.

## Validation

The final required checks passed: DSP 27.26 s, UI retest 15.61 s,
RealtimeMatrix 386.21 s, NapValidation 215.86 s. UI also passed five consecutive
additional runs (64.93 s total) and a debugger run. The new eight-scene spectrum
checks are part of those UI runs. The screenshot shows ALL on bands 1/3 and NO
on bands 2/4, alongside the independent original input trace.

The initial complete run had a UI process exit 0xc0000409. It has not reproduced;
its cause is unresolved. Only progress logging was added to the test executable,
so this report does not claim a production fix. The initial failed result,
debugger log, successful retest and repeated-run logs are retained separately.

The callback audit reports 4,320 scenes / 43,200 callbacks plus 1,024 concurrent
callbacks. FIR audit covers 31,104 callbacks. All measured allocation, free,
lock, wait and I/O counts are zero; both audio-reference errors are zero.
This audits imports of the diagnostic executable, not all DAW/OS activity,
and is not a host CPU benchmark. The independent IIR audio/plot comparison
still has maximum error 0.00151437 dB for magnitudes above -80 dB.

Build: MSVC Release, Windows x64, JUCE 9.0.1. The VST3 was built successfully.
Studio One 6 manual routing and this new overlay on Linux/macOS have not been
verified in this task. No installation or publication was performed.

Artifacts live in ../../../artifacts/MC2000/0.2.5-sidechain-spectrum-2026-10-08:
the VST3 ZIP, source archive, source hashes, screenshot, test reports,
realtime JSON, nap CSV and manifest. Earlier artifact buckets remain untouched.

Packaged VST3 SHA-256: F78CE6C3BE23078A49DE3DAB7AC611A7B786EA6C2BA02042C99E1756318AE2FA

**Prepared v0.2.2 notes - publication awaits user OK.**

Ponte MBC4 v0.2.2 fixes short peaks disappearing between GUI updates and the overly fast meter return. IN, OUT and stereo MAIN OUTPUT now retain peaks and use the measured level-display profile; GR has a separate 150 ms visual return. Compression formulas and compressor release remain unchanged, with no artificial GR offset. DSP_MODEL_5 makes IN an independent input mute: IN off silences the band and detector, even when SOLO is on. Switching uses a 5 ms exponential time constant to avoid clicks. Existing sessions with IN off change sound; stored parameter values are preserved.

This release also includes the UI refinements since v0.2.0: editable knob-value overlays, consistent hover/keyboard focus, clearer labels, independent IN and SOLO controls, persistent editor size, aligned MAIN OUTPUT and CROSSOVER, larger contextual help and -60..0 dB meter/graph scales. Knob double-click opens numeric editing without reset; active SOLO has lime text/outline on ink fill. STATIC I/O points share the band-meter ballistics. The stereo FFT uses the same level-display ballistics, retains crossover filter tails and discards stale queued audio.

The research pack, capture reports, POC and meter technical note document measurements and limitations. The shared CompanyGUI profile has been updated in the PonteDSP workspace.

Validation: Windows x64 Release build, both DSP and GUI suites passed (2/2), numerical GR-model checks passed, and all 35 source recordings verified unchanged. Windows file-version resources also refresh correctly on incremental version bumps. Automated coverage includes peak retention at 64/512/1024 samples, concurrent peak exchange, stereo independence, unchanged audio under GUI polling, elapsed-time smoothing, repaint, callback suspension and editor reopening.

Remaining validation: record the new GUI in Ableton using tests 01/02 and 03/04 R250. Re-export neutral B0 references wherever IN-off routing changes the audio. The original's GR plateau difference, R500 and the real-voice comparison remain open. The display model is a measured approximation, not a claim of identical McDSP internals.

Install the complete `Ponte MBC4.vst3` bundle from the Windows ZIP in your VST3 directory with the DAW closed, then rescan plugins. Build identity and binary SHA-256 are included in `BUILD_INFO.txt`.

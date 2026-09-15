Ponte MBC4 v0.2.2 fixes short peaks disappearing between GUI updates and the overly fast meter return. IN, OUT and stereo MAIN OUTPUT now retain peaks and use the measured level-display profile; GR has a separate 150 ms visual return. Audio processing remains DSP_MODEL_4, with no change to compressor release or artificial GR offset.

This release also includes the UI refinements since v0.2.0: editable knob-value overlays, consistent hover/keyboard focus, clearer labels, preserved IN settings during SOLO, persistent editor size, aligned MAIN OUTPUT and CROSSOVER, larger contextual help and a 0 to -48 dB band display.

The research pack, capture reports, POC and meter technical note document measurements and limitations. The shared CompanyGUI profile has been updated in the PonteDSP workspace.

Validation: Windows x64 Release build, both DSP and GUI suites passed (2/2), numerical GR-model checks passed, and all 35 source recordings verified unchanged. Windows file-version resources also refresh correctly on incremental version bumps. Automated coverage includes peak retention at 64/512/1024 samples, concurrent peak exchange, stereo independence, unchanged audio under GUI polling, elapsed-time smoothing, repaint, callback suspension and editor reopening.

Remaining validation: record the new GUI in Ableton using tests 01/02 and 03/04 R250. The original's GR plateau difference, R500 and the real-voice comparison remain open. The display model is a measured approximation, not a claim of identical McDSP internals.

Install the complete `Ponte MBC4.vst3` bundle from the Windows ZIP in your VST3 directory with the DAW closed, then rescan plugins. Build identity and binary SHA-256 are included in `BUILD_INFO.txt`.

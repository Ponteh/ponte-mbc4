# Ponte MBC4

Clean-room multiband compressor POC derived from the behavioural specification
in `MC2000_Proof_of_Concept.md`.

The production DSP in `Source/DSP` has no JUCE dependency. JUCE is confined to
the plug-in wrapper and user interface. `DSP_MODEL_2` fits the
measured LR4 crossover, stereo detector, full Knee curves, Attack, Type-1,
Type-2 and BITE behaviour to the 49 available original plug-in renders. See
`Research/BLACK_BOX_FIT.md` for measured errors and the remaining limits around
Auto and isolated four-band internal phase.

`MC2000` remains the internal research codename. The public product name and
plug-in identity are `Ponte MBC4`, avoiding third-party product identity.

UI changes and build history are catalogued in
[`shared/CompanyGUI/MC2000_UI_CATALOG.md`](../../shared/CompanyGUI/MC2000_UI_CATALOG.md).
Meter scales and control captions use the same white as R1; the last band
selected by mouse, wheel or keyboard focus is drawn in front in Static I/O.
The master output meter matches the width of Static I/O, with MAIN OUTPUT to the left of the bars.
Knob values appear in an editable overlay above the control on interaction
or keyboard focus, or after 30 ms of initial hover. Handoffs between controls
are immediate; visual focus clears after 100 ms outside controls, unless dragging,
editing or using a menu. Knobs, selectors, buttons and numeric fields share
one visual focus controller. Captions sit below the knobs;
no layout space is reserved for hidden values. Active knobs use a slightly brighter
accent; unfocused controls retain their original saturation. See POC sections
71–73 for the interaction contract. SOLO temporarily overrides the displayed
IN states without overwriting them: every IN button is off and disabled during
SOLO, including the isolated bands. Releasing the last SOLO restores the saved
IN configuration. The initial editor size is 1100 × 738; resizing is preserved
across band-count changes, editor reopening and session save/restore.

CROSSOVER starts at the left edge of the band display. The context help area
above the input/output knobs is 168 × 54, with up to four full-size text lines.
Band responses and the spectrum share a vertical display range of -48 dB at
the bottom to 0 dB at the top; crossover markers sit on the 0 dB line.

Current public parameter ranges are: Input/Output and per-band Gain
`-24.0..+24.0 dB`, Threshold `-48.0..0.0 dB`, Ratio `1.00:1..10.00:1`,
Knee `-10.00..+15.00`, BITE `1.00..10.00`, Attack `0.25..250.00 ms`, and
Release `25.0..2500.0 ms`.

`Research/NEXT_VALIDATION_AUDIT.md` records the latest Auto, BITE, stereo and
sample-rate render audit. `MC2000NextPackAnalysis` is the companion executable
for rerunning it against a clean validation-pack root.

Build on Windows. A standalone clone downloads the pinned JUCE 9.0.1 source
during the first configure; inside the PonteDSP monorepo it reuses the local
JUCE checkout. An out-of-tree build path is recommended:

```powershell
cmake -S . -B C:/build/PonteMBC4 -A x64
cmake --build C:/build/PonteMBC4 --config Release --target PonteMC2000_VST3 MC2000Tests MC2000UITests
ctest --test-dir C:/build/PonteMBC4 -C Release --output-on-failure
```

`MC2000UITests` exercises focus timing/controller handoffs, numeric editing,
the real editor's SOLO/IN buttons and automation, state persistence, resize,
meter alignment and audio routing against the sum of selected crossover bands.
It writes `MC2000_UI_minimum.png` beside the test executable for visual review.

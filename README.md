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

Build on Windows. A standalone clone downloads the pinned JUCE 9.0.1 source
during the first configure; inside the PonteDSP monorepo it reuses the local
JUCE checkout. An out-of-tree build path is recommended:

```powershell
cmake -S . -B C:/build/PonteMBC4 -A x64
cmake --build C:/build/PonteMBC4 --config Release --target PonteMC2000_VST3 MC2000Tests
ctest --test-dir C:/build/PonteMBC4 -C Release --output-on-failure
```

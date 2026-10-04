# Mandatory correctness tests

Configure with `MC2000_BUILD_TESTS=ON` (default). Build these targets:

```sh
cmake --build build --config Release --target PonteMC2000_VST3 MC2000Tests MC2000UITests MC2000NapValidation
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

Expected CTest suite: DSP, UI and NapValidation on every platform (3 tests),
plus RealtimeMatrix on Windows (4). Linux GUI tests require Xvfb and a window
manager; use the workspace Unix build helper. No Research directory, recordings
or acquisition ZIP files are needed. A missing NapValidation source is a build
configuration error, never a reason to silently omit the test.

NapValidation restores the regression suite from commit `5b9249c`:

- 44.1/48/96/192 kHz, 64/512/2048-sample blocks, R1/R2/Auto modes;
- nap-on/off equivalence, drained silence and wake-up, external sidechain;
- tiny signals, changed parameters/routing and unchanged parameter snapshots;
- all 512 impulse positions, right-channel-only input;
- a 100-second signal/tail sequence with 2500 ms release.

It writes `nap-validation.csv` in the test working directory (optional first
argument: report path). It contains no timing benchmark. The denormal guard
uses `juce::ScopedNoDenormals` on both Intel and ARM64 and checks activation and
restoration of the previous denormal mode. Cross-compiling a Universal binary
does not test ARM execution; an Apple Silicon run is still required for that.

Timing experiments live in `Research`, behind `MC2000_BUILD_RESEARCH=ON`.
See `Research/README.md` when that optional directory is present.

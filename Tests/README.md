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

## Release 0.2.5 coverage

The existing targets also check measured digital IIR response and phase,
independent direct FIR impulse convolution, FIR curve and group delay, complementary
reconstruction, worker request coalescing and bounded warm-history transitions.
Dual Mono channels are compared against independent mono compressors for key
layouts, band counts and TC/BITE modes. Wrapper/UI checks cover appended parameter
indices, schema migration, undo/redo, pending mode, latency, oversized native bypass
and minimum-size selectors. Nap includes FIR tails and right-only wake at four
rates. The Windows realtime audit adds both channel modes and the FIR backend
with irregular/oversized blocks and nonfinite inputs. No new mandatory CTest target
or dependency on Research has been introduced.
## Where the new checks live

| File | Checks |
| --- | --- |
| `CrossoverAndChannelModeTests.h` | Measured IIR response, complementary FIR sum, independent Dual Mono references, channel change and nap |
| `LinearPhaseCrossoverTests.h` | Direct impulse convolution, FIR magnitude/phase/group delay, worker requests and kernel fade |
| `CrossoverStateLatencyAndUITests.h` | Host parameter order, legacy migration, undo/redo, session restore, PDC/bypass and minimum GUI |
| `LinearPhaseNapValidation.h` | FIR tail drainage and right-only wake at four sample rates |
| `LinearPhaseRealtimeValidation.h` | Audited callbacks, layouts, irregular blocks, native bypass and kernel update with GUI response reads |

These are included by the existing DSP, UI and Nap executables. Their names
identify the behavior; release numbers belong to validation reports, not test filenames.

IntelliSense must parse this product as C++20: defaulted comparison operators
and `std::numbers` are C++20 facilities. The workspace and product `.vscode`
configurations select C++20/MSVC; CMake has always required C++20. On another
Windows machine update `compilerPath` to the installed MSVC compiler or use
CMake Tools as the configuration provider. After an IDE cache persists old
errors, run **C/C++: Reset IntelliSense Database** and reopen the affected file.

# Optional research tools

The plugin and all correctness tests build without this directory or any audio
acquisition archive. `MC2000_BUILD_RESEARCH` defaults to `OFF`.

To build experiments explicitly:

```sh
cmake -S . -B build-research -DMC2000_BUILD_RESEARCH=ON -DMC2000_BUILD_TESTS=OFF
cmake --build build-research --config Release --target MC2000OversamplingCost MC2000NapBenchmark
```

`MC2000OversamplingCost` replaces the old UI-test `--oversampling-cost` switch;
`MC2000NapBenchmark` replaces NapValidation's `--benchmark` switch. Neither is
registered with CTest. Oversampling remains an experiment, not a plugin feature.

Archived analyzers (PerformanceBenchmark, OriginalPackRender, BlackBoxAnalysis,
ExtendedPackAnalysis, NextPackAnalysis) are available only when their source
files are restored here AND research is enabled. They may need their own data
and helpers; normal builds never unpack or require those archives.

Nap correctness lives in `Tests/NapValidation.cpp` and is mandatory whenever
`MC2000_BUILD_TESTS=ON`, with a 600-second timeout and `nap-validation.csv` output.
The floating-point guard uses JUCE's x86/ARM64 implementation; the test checks
that denormal suppression is active and that the guard restores previous state.

## Release 0.2.5 experiments

`MC2000LinearPhaseBenchmark`: symmetry, 20 Hz pass/stop regions, complementary
impulse sum, direct convolution reference and direct/partitioned cost.
`MC2000LinearPhaseCompressorBenchmark [csv]`: complete compressor with stereo key, four
sample rates, normal/20-21-22 Hz crossovers, both channel modes and three blocks;
reports average cost, p99, maximum and deadline. Run with no compiler load.
`MC2000CompatibilityRender output.raw`: deterministic IIR/Stereo renders for
comparing an independently compiled frozen 0.2.3 baseline with the candidate.
These targets are optional and do not alter the mandatory CTest suite.

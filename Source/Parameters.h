#pragma once

#include "DSP/MultiBandCompressor.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pontedsp::mc2000::parameters {

inline constexpr int stateSchemaVersion = 2;
inline constexpr const char* inputGain = "global.inputGainDb";
inline constexpr const char* outputGain = "global.outputGainDb";
inline constexpr const char* phaseInvert = "global.phaseInvert";
inline constexpr const char* bandCount = "global.bandCount";
inline constexpr const char* linkMaster = "global.linkMaster";

juce::String crossoverId(int index);
juce::String bandId(int band, const char* suffix);
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

struct LinkRuntime
{
    static constexpr int linkedParameters = 7;
    int masterBand { -1 };
    bool initialised {};
    std::array<std::array<double, linkedParameters>, 4> offsets {};
    std::array<std::array<double, linkedParameters>, 4> previousRaw {};
};

// APVTS owns these atomics for its lifetime, including replaceState(). Resolve
// IDs once on construction, not by allocating parameter-name strings in audio.
class SnapshotReader final
{
public:
    explicit SnapshotReader(const juce::AudioProcessorValueTreeState&);
    dsp::GlobalParameters read(LinkRuntime&) const noexcept;
private:
    std::array<std::atomic<float>*, 5> globals {};
    std::array<std::atomic<float>*, 3> crossovers {};
    std::array<std::array<std::atomic<float>*, 10>, 4> bands {};
};

dsp::GlobalParameters readSnapshot(const juce::AudioProcessorValueTreeState& state,
                                   LinkRuntime& linkRuntime) noexcept;

} // namespace pontedsp::mc2000::parameters

#pragma once

#include "DSP/MultiBandCompressor.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace pontedsp::mc2000::parameters {

inline constexpr int stateSchemaVersion = 1;
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

dsp::GlobalParameters readSnapshot(const juce::AudioProcessorValueTreeState& state,
                                   LinkRuntime& linkRuntime) noexcept;

} // namespace pontedsp::mc2000::parameters


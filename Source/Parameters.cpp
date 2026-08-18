#include "Parameters.h"

namespace pontedsp::mc2000::parameters {
namespace {

float value(const juce::AudioProcessorValueTreeState& state, const juce::String& id) noexcept
{
    if (const auto* raw = state.getRawParameterValue(id))
        return raw->load(std::memory_order_relaxed);
    return 0.0f;
}

juce::NormalisableRange<float> logarithmicRange(const float minimum, const float maximum,
                                                 const float centre)
{
    juce::NormalisableRange<float> range(minimum, maximum);
    range.setSkewForCentre(centre);
    return range;
}

constexpr std::array<const char*, LinkRuntime::linkedParameters> linkedSuffixes {
    "gainDb", "thresholdDb", "ratio", "knee", "bite", "attackMs", "releaseMs"
};

constexpr std::array<double, LinkRuntime::linkedParameters> minima {
    0.0, -45.0, 1.0, -10.0, 1.0, 0.03, 5.0
};

constexpr std::array<double, LinkRuntime::linkedParameters> maxima {
    48.0, 0.0, 10.0, 15.0, 50.0, 250.0, 2500.0
};

} // namespace

juce::String crossoverId(const int index)
{
    return "xover." + juce::String(index + 1) + ".frequencyHz";
}

juce::String bandId(const int band, const char* suffix)
{
    return "band" + juce::String(band + 1) + "." + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(inputGain, 1), "Input Gain", -24.0f, 24.0f, 0.0f));
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(outputGain, 1), "Output Gain", -24.0f, 24.0f, 0.0f));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(phaseInvert, 1), "Phase Invert", false));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(bandCount, 1), "Band Count", juce::StringArray { "2", "3", "4" }, 2));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(linkMaster, 1), "Link Master",
        juce::StringArray { "Unlinked", "Master 1", "Master 2", "Master 3", "Master 4" }, 0));

    const std::array<float, 3> crossoverDefaults { 100.0f, 1000.0f, 10000.0f };
    for (int crossover = 0; crossover < 3; ++crossover)
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(crossoverId(crossover), 1), "Crossover " + juce::String(crossover + 1),
            logarithmicRange(20.0f, 20000.0f, 1000.0f), crossoverDefaults[static_cast<std::size_t>(crossover)]));

    for (int band = 0; band < 4; ++band)
    {
        const auto number = juce::String(band + 1);
        layout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandId(band, "enabled"), 1), "Band " + number + " In", true));
        layout.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(bandId(band, "solo"), 1), "Band " + number + " Solo", false));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "gainDb"), 1), "Band " + number + " Gain", 0.0f, 48.0f, 0.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "thresholdDb"), 1), "Band " + number + " Threshold", -45.0f, 0.0f, 0.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "ratio"), 1), "Band " + number + " Ratio", 1.0f, 10.0f, 1.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "knee"), 1), "Band " + number + " Knee", -10.0f, 15.0f, 0.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "bite"), 1), "Band " + number + " Bite", 1.0f, 50.0f, 1.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "attackMs"), 1), "Band " + number + " Attack",
            logarithmicRange(0.03f, 250.0f, 10.0f), 10.0f));
        layout.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(bandId(band, "releaseMs"), 1), "Band " + number + " Release",
            logarithmicRange(5.0f, 2500.0f, 250.0f), 250.0f));
        layout.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(bandId(band, "tcMode"), 1), "Band " + number + " Time Constant",
            juce::StringArray { "R1", "R2", "Auto" }, 0));
    }

    return { layout.begin(), layout.end() };
}

dsp::GlobalParameters readSnapshot(const juce::AudioProcessorValueTreeState& state,
                                   LinkRuntime& runtime) noexcept
{
    dsp::GlobalParameters snapshot;
    snapshot.inputGainDb = value(state, inputGain);
    snapshot.outputGainDb = value(state, outputGain);
    snapshot.phaseInvert = value(state, phaseInvert) > 0.5f;
    snapshot.numBands = std::clamp(static_cast<int>(value(state, bandCount)) + 2, 2, 4);
    for (int crossover = 0; crossover < 3; ++crossover)
        snapshot.crossoverHz[static_cast<std::size_t>(crossover)] = value(state, crossoverId(crossover));
    snapshot.crossoverHz[0] = std::clamp(snapshot.crossoverHz[0], 20.0, 18000.0);
    snapshot.crossoverHz[1] = std::clamp(snapshot.crossoverHz[1], snapshot.crossoverHz[0] + 1.0, 19000.0);
    snapshot.crossoverHz[2] = std::clamp(snapshot.crossoverHz[2], snapshot.crossoverHz[1] + 1.0, 20000.0);

    std::array<std::array<double, LinkRuntime::linkedParameters>, 4> raw {};
    for (int band = 0; band < 4; ++band)
    {
        auto& p = snapshot.bands[static_cast<std::size_t>(band)];
        p.enabled = value(state, bandId(band, "enabled")) > 0.5f;
        p.solo = value(state, bandId(band, "solo")) > 0.5f;
        for (int parameter = 0; parameter < LinkRuntime::linkedParameters; ++parameter)
            raw[static_cast<std::size_t>(band)][static_cast<std::size_t>(parameter)] =
                value(state, bandId(band, linkedSuffixes[static_cast<std::size_t>(parameter)]));
        p.tcMode = static_cast<dsp::TCMode>(std::clamp(
            static_cast<int>(value(state, bandId(band, "tcMode"))), 0, 2));
    }

    const auto selectedMaster = std::clamp(static_cast<int>(value(state, linkMaster)) - 1, -1, 3);
    const auto sourceRaw = raw;
    if (!runtime.initialised || selectedMaster != runtime.masterBand)
    {
        runtime.masterBand = selectedMaster;
        runtime.previousRaw = raw;
        runtime.initialised = true;
        if (selectedMaster >= 0)
            for (int band = 0; band < 4; ++band)
                for (int parameter = 0; parameter < LinkRuntime::linkedParameters; ++parameter)
                    runtime.offsets[static_cast<std::size_t>(band)][static_cast<std::size_t>(parameter)] =
                        raw[static_cast<std::size_t>(band)][static_cast<std::size_t>(parameter)]
                      - raw[static_cast<std::size_t>(selectedMaster)][static_cast<std::size_t>(parameter)];
    }

    if (selectedMaster >= 0)
    {
        for (int band = 0; band < 4; ++band)
            for (int parameter = 0; parameter < LinkRuntime::linkedParameters; ++parameter)
            {
                const auto b = static_cast<std::size_t>(band);
                const auto p = static_cast<std::size_t>(parameter);
                if (band != selectedMaster && raw[b][p] != runtime.previousRaw[b][p])
                    runtime.offsets[b][p] = raw[b][p] - raw[static_cast<std::size_t>(selectedMaster)][p];
                raw[b][p] = std::clamp(raw[static_cast<std::size_t>(selectedMaster)][p]
                                     + runtime.offsets[b][p], minima[p], maxima[p]);
            }
        const auto masterMode = snapshot.bands[static_cast<std::size_t>(selectedMaster)].tcMode;
        for (auto& band : snapshot.bands) band.tcMode = masterMode;
    }
    runtime.previousRaw = sourceRaw;

    for (int band = 0; band < 4; ++band)
    {
        auto& p = snapshot.bands[static_cast<std::size_t>(band)];
        const auto& r = raw[static_cast<std::size_t>(band)];
        p.gainDb = r[0]; p.thresholdDb = r[1]; p.ratio = r[2]; p.knee = r[3];
        p.bite = r[4]; p.attackMs = r[5]; p.releaseMs = r[6];
    }
    return snapshot;
}

} // namespace pontedsp::mc2000::parameters

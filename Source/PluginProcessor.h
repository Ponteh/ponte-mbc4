#pragma once

#include "DSP/MultiBandCompressor.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PonteMC2000AudioProcessor final : public juce::AudioProcessor
{
public:
    PonteMC2000AudioProcessor();
    ~PonteMC2000AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int size) override;

    pontedsp::mc2000::dsp::MultiBandCompressor& getEngine() noexcept { return engine; }
    const pontedsp::mc2000::dsp::MultiBandCompressor& getEngine() const noexcept { return engine; }
    using SpectrumSample = std::array<float, 2>;
    int popSpectrumSamples(SpectrumSample* destination, int maximumSamples, bool& discontinuity) noexcept;
    void discardSpectrumSamples() noexcept;
    double getProcessingSampleRate() const noexcept { return processingSampleRate.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState state;
    std::atomic<int> editorWidth { 1100 }, editorHeight { 738 };

private:
    void pushSpectrumSamples(const juce::AudioBuffer<float>&) noexcept;

    static constexpr int spectrumFifoCapacity = 32768;
    pontedsp::mc2000::dsp::MultiBandCompressor engine;
    pontedsp::mc2000::parameters::LinkRuntime linkRuntime;
    std::array<SpectrumSample, spectrumFifoCapacity> spectrumSamples {};
    std::atomic<bool> spectrumOverflow { false };
    juce::AbstractFifo spectrumFifo { spectrumFifoCapacity };
    std::atomic<double> processingSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PonteMC2000AudioProcessor)
};

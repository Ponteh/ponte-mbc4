#include "PluginProcessor.h"
#include "PluginEditor.h"

PonteMC2000AudioProcessor::PonteMC2000AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PONTE_MC2000_STATE", pontedsp::mc2000::parameters::createLayout())
{
}

void PonteMC2000AudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    spectrumFifo.reset();
    spectrumOverflow.store(false, std::memory_order_relaxed);
    processingSampleRate.store(sampleRate, std::memory_order_relaxed);
    engine.setParameters(parameterReader.read(linkRuntime));
    engine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

bool PonteMC2000AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    const auto sidechain = layouts.getChannelSet(true, 1);
    const auto validMain = (output == juce::AudioChannelSet::mono()
                         || output == juce::AudioChannelSet::stereo())
                        && output == layouts.getMainInputChannelSet();
    const auto validSidechain = sidechain.isDisabled()
                            || sidechain == juce::AudioChannelSet::mono()
                            || sidechain == juce::AudioChannelSet::stereo();
    return validMain && validSidechain;
}

void PonteMC2000AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    engine.setParameters(parameterReader.read(linkRuntime));

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
    pushSpectrumSamples(mainBuffer);
    std::array<float*, 2> program { mainBuffer.getWritePointer(0), nullptr };
    if (mainBuffer.getNumChannels() > 1) program[1] = mainBuffer.getWritePointer(1);

    std::array<const float*, 2> detector { nullptr, nullptr };
    if (sidechainBuffer.getNumChannels() > 0)
    {
        detector[0] = sidechainBuffer.getReadPointer(0);
        if (sidechainBuffer.getNumChannels() > 1) detector[1] = sidechainBuffer.getReadPointer(1);
    }
    engine.process(program.data(), mainBuffer.getNumChannels(), detector.data(),
                   sidechainBuffer.getNumChannels(), mainBuffer.getNumSamples());
}

void PonteMC2000AudioProcessor::pushSpectrumSamples(const juce::AudioBuffer<float>& buffer) noexcept
{
    if (spectrumConsumers.load(std::memory_order_acquire) == 0) return;
    const auto channels = buffer.getNumChannels();
    if (channels <= 0) return;

    int start1 {}, size1 {}, start2 {}, size2 {};
    spectrumFifo.prepareToWrite(buffer.getNumSamples(), start1, size1, start2, size2);
    const auto writeRange = [&] (const int fifoStart, const int count, const int sourceStart)
    {
        for (int sample = 0; sample < count; ++sample)
        {
            const auto left = buffer.getSample(0, sourceStart + sample);
            const auto right = buffer.getSample(std::min(1, channels - 1), sourceStart + sample);
            spectrumSamples[static_cast<std::size_t>(fifoStart + sample)] = {
                std::isfinite(left) ? left : 0.0f, std::isfinite(right) ? right : 0.0f };
        }
    };
    writeRange(start1, size1, 0);
    writeRange(start2, size2, size1);
    spectrumFifo.finishedWrite(size1 + size2);
    if (size1 + size2 < buffer.getNumSamples())
        spectrumOverflow.store(true, std::memory_order_release);
}

void PonteMC2000AudioProcessor::discardSpectrumSamples() noexcept
{
    spectrumOverflow.exchange(false, std::memory_order_acquire);
    int start1 {}, size1 {}, start2 {}, size2 {};
    spectrumFifo.prepareToRead(spectrumFifo.getNumReady(), start1, size1, start2, size2);
    spectrumFifo.finishedRead(size1 + size2);
}

int PonteMC2000AudioProcessor::popSpectrumSamples(SpectrumSample* const destination,
                                                   const int maximumSamples,
                                                   bool& discontinuity) noexcept
{
    discontinuity = false;
    if (destination == nullptr || maximumSamples <= 0) return 0;
    if (spectrumOverflow.exchange(false, std::memory_order_acquire))
    {
        // The queue may contain audio from before a closed/stalled editor.
        // Drain it instead of replaying old sound; the next callback is fresh.
        discardSpectrumSamples();
        discontinuity = true;
        return 0;
    }
    const auto ready = spectrumFifo.getNumReady();
    if (ready > maximumSamples)
    {
        int start1 {}, size1 {}, start2 {}, size2 {};
        spectrumFifo.prepareToRead(ready - maximumSamples, start1, size1, start2, size2);
        spectrumFifo.finishedRead(size1 + size2);
        discontinuity = true;
    }
    int start1 {}, size1 {}, start2 {}, size2 {};
    spectrumFifo.prepareToRead(maximumSamples, start1, size1, start2, size2);
    std::copy_n(spectrumSamples.data() + start1, size1, destination);
    std::copy_n(spectrumSamples.data() + start2, size2, destination + size1);
    spectrumFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

juce::AudioProcessorEditor* PonteMC2000AudioProcessor::createEditor()
{
    return new PonteMC2000AudioProcessorEditor(*this);
}

void PonteMC2000AudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto saved = state.copyState();
    saved.setProperty("editorWidth", editorWidth.load(), nullptr);
    saved.setProperty("editorHeight", editorHeight.load(), nullptr);
    saved.setProperty("schemaVersion", pontedsp::mc2000::parameters::stateSchemaVersion, nullptr);
    saved.setProperty("dspModelVersion", pontedsp::mc2000::dsp::MultiBandCompressor::dspModelVersion, nullptr);
    if (const auto xml = saved.createXml())
        copyXmlToBinary(*xml, destination);
}

void PonteMC2000AudioProcessor::setStateInformation(const void* data, const int size)
{
    if (const auto xml = getXmlFromBinary(data, size))
    {
        auto restored = juce::ValueTree::fromXml(*xml);
        if (restored.hasType(state.state.getType()))
        {
            editorWidth.store(juce::jlimit(1100, 1600, static_cast<int>(restored.getProperty("editorWidth", 1100))));
            editorHeight.store(juce::jlimit(738, 1100, static_cast<int>(restored.getProperty("editorHeight", 738))));
            state.replaceState(restored);
            linkRuntime = {};
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PonteMC2000AudioProcessor();
}

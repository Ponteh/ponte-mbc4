#include "PluginProcessor.h"
#include "PluginEditor.h"

PonteMC2000AudioProcessor::PonteMC2000AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PONTE_MC2000_STATE", pontedsp::mc2000::parameters::createLayout())
{
}

void PonteMC2000AudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
    engine.setParameters(pontedsp::mc2000::parameters::readSnapshot(state, linkRuntime));
    engine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

bool PonteMC2000AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layouts.getMainInputChannelSet();
}

void PonteMC2000AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    engine.setParameters(pontedsp::mc2000::parameters::readSnapshot(state, linkRuntime));
    std::array<float*, 2> pointers { buffer.getWritePointer(0), nullptr };
    if (buffer.getNumChannels() > 1) pointers[1] = buffer.getWritePointer(1);
    engine.process(pointers.data(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* PonteMC2000AudioProcessor::createEditor()
{
    return new PonteMC2000AudioProcessorEditor(*this);
}

void PonteMC2000AudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto saved = state.copyState();
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
            state.replaceState(restored);
            linkRuntime = {};
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PonteMC2000AudioProcessor();
}


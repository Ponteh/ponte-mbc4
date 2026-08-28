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
    engine.setParameters(pontedsp::mc2000::parameters::readSnapshot(state, linkRuntime));
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
    engine.setParameters(pontedsp::mc2000::parameters::readSnapshot(state, linkRuntime));

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
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

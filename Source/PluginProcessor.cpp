#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Diagnostics.h"

PonteMC2000AudioProcessor::PonteMC2000AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, &undoManager, "PONTE_MC2000_STATE", pontedsp::mc2000::parameters::createLayout())
{
    requestedCrossoverMode=state.getRawParameterValue(pontedsp::mc2000::parameters::crossoverMode);
    startTimerHz(30);
}
PonteMC2000AudioProcessor::~PonteMC2000AudioProcessor()
{
    stopTimer();stopReload.store(true,std::memory_order_release);
    if(reloadThread.joinable())reloadThread.join();
    cancelPendingUpdate();
}

void PonteMC2000AudioProcessor::waitForCallbacks()
{
    while(callbackReaders.load(std::memory_order_seq_cst)!=0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void PonteMC2000AudioProcessor::releaseResources()
{
    std::lock_guard<std::mutex> lock(configurationMutex);
    prepared.store(false,std::memory_order_release);
    configurationGeneration.fetch_add(1,std::memory_order_seq_cst);
    reloadState.store(ReloadState::loading,std::memory_order_seq_cst);
    waitForCallbacks();
    // Retain allocated resources. Host reactivation prepares the next configuration.
    preparing.store(false,std::memory_order_release);
    reloadState.store(ReloadState::idle,std::memory_order_seq_cst);
}

void PonteMC2000AudioProcessor::configureEngine(const double newSampleRate,const int samplesPerBlock,const int mainChannels)
{


    const double sampleRate = pontedsp::mc2000::dsp::clampFinite(newSampleRate,8000.0,384000.0,48000.0);
    spectrumFifo.reset();
    spectrumOverflow.store(false, std::memory_order_relaxed);
    if (resetLinkRuntime.exchange(false, std::memory_order_acquire)) linkRuntime = {};
    engine.setParameters(parameterReader.read(linkRuntime));
    engine.prepare(sampleRate, samplesPerBlock, mainChannels);
    const int latency = engine.getLatencySamples();
    activeLatency.store(latency,std::memory_order_release);
    activeCrossoverMode.store(static_cast<int>(engine.getActiveCrossoverMode()), std::memory_order_relaxed);
    linearTailSeconds.store((pontedsp::mc2000::dsp::LinearPhaseCrossover::profileTaps(sampleRate) - 1 + latency - (pontedsp::mc2000::dsp::LinearPhaseCrossover::profileTaps(sampleRate) - 1) / 2) / sampleRate, std::memory_order_relaxed);
    for (auto& delay : bypassDelay) delay.assign(static_cast<std::size_t>(latency + 1), 0.0f);
    for (auto& work : bypassWork) work.resize(static_cast<std::size_t>(std::max(1, samplesPerBlock)));
    bypassPosition = 0;
    processingSampleRate.store(sampleRate, std::memory_order_relaxed);


    preparedBlock.store(std::max(1,samplesPerBlock),std::memory_order_relaxed);
    preparedMainChannels.store(mainChannels,std::memory_order_relaxed);
}
void PonteMC2000AudioProcessor::prepareToPlay(const double rate,const int block)
{
    {
        std::lock_guard<std::mutex> lock(configurationMutex);
        configurationGeneration.fetch_add(1,std::memory_order_seq_cst);
        reloadState.store(ReloadState::loading,std::memory_order_seq_cst);
        preparing.store(true,std::memory_order_release);
        waitForCallbacks();
        configureEngine(rate,block,getMainBusNumInputChannels());
        reloadGain=1;reloadWarmSamples=0;lastFadeState=ReloadState::idle;
        prepared.store(true,std::memory_order_release);
        preparing.store(false,std::memory_order_release);
        reloadState.store(ReloadState::idle,std::memory_order_seq_cst);
    }
    // Hosts may synchronously prepare again from a latency notification, possibly
    // with a newer requested mode. Finish configuration and release its mutex first.
    setLatencySamples(getActiveLatencySamples());
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
    CallbackAccess access(*this);
    if(!access.allowed){buffer.clear();return;}
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    if (resetLinkRuntime.exchange(false, std::memory_order_acquire)) linkRuntime = {};
    { MC2000_MEASURE(snapshot); engine.setParameters(parameterReader.read(linkRuntime)); }

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
    pushSpectrumSamples(mainBuffer, sidechainBuffer);
    std::array<float*, 2> program { mainBuffer.getWritePointer(0), nullptr };
    if (mainBuffer.getNumChannels() > 1) program[1] = mainBuffer.getWritePointer(1);

    std::array<const float*, 2> detector { nullptr, nullptr };
    if (sidechainBuffer.getNumChannels() > 0)
    {
        detector[0] = sidechainBuffer.getReadPointer(0);
        if (sidechainBuffer.getNumChannels() > 1) detector[1] = sidechainBuffer.getReadPointer(1);
    }
    for (int sample = 0; sample < mainBuffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            const float raw = mainBuffer.getSample(channel, sample);
            bypassDelay[static_cast<std::size_t>(channel)][static_cast<std::size_t>(bypassPosition)] = std::isfinite(raw) ? raw : 0.0f;
        }
        bypassPosition = (bypassPosition + 1) % static_cast<int>(bypassDelay[0].size());
    }
    MC2000_MEASURE(dsp);
    engine.process(program.data(), mainBuffer.getNumChannels(), detector.data(),
                   sidechainBuffer.getNumChannels(), mainBuffer.getNumSamples());
    applyReloadFade(mainBuffer);
}

void PonteMC2000AudioProcessor::pushSpectrumSamples(const juce::AudioBuffer<float>& buffer,
                                                     const juce::AudioBuffer<float>& key) noexcept
{
    MC2000_MEASURE(fifo);
    if (spectrumConsumers.load(std::memory_order_acquire) == 0) return;
    const auto channels = buffer.getNumChannels();
    if (channels <= 0) return;

    unsigned keyMask = 0;
    const auto& parameters = engine.getParameters();
    if (key.getNumChannels() > 0)
        for (int band = 0; band < parameters.numBands; ++band)
            if (parameters.bands[std::size_t(band)].enabled
                && parameters.bands[std::size_t(band)].sidechainSource
                    == pontedsp::mc2000::dsp::SidechainSource::all)
                keyMask |= 1u << band;
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(std::min(1, channels - 1));
    const float* keyLeft = keyMask != 0 ? key.getReadPointer(0) : nullptr;
    const float* keyRight = keyMask != 0 ? key.getReadPointer(std::min(1, key.getNumChannels() - 1)) : nullptr;
    const auto finite = [](float value) noexcept { return std::isfinite(value) ? value : 0.0f; };

    int start1 {}, size1 {}, start2 {}, size2 {};
    spectrumFifo.prepareToWrite(buffer.getNumSamples(), start1, size1, start2, size2);
    const auto writeRange = [&] (const int fifoStart, const int count, const int sourceStart)
    {
        for (int sample = 0; sample < count; ++sample)
        {
            const int position = sourceStart + sample;
            spectrumSamples[std::size_t(fifoStart + sample)] = {
                { finite(left[position]), finite(right[position]),
                  keyLeft ? finite(keyLeft[position]) : 0.0f,
                  keyRight ? finite(keyRight[position]) : 0.0f }, keyMask };
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
    // Opening or closing an editor can race the reload worker's FIFO reset.
    ResponseReadAccess access(*this);
    if (!access.allowed || isPreparing()) return;
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

void PonteMC2000AudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    CallbackAccess access(*this);
    if(!access.allowed){buffer.clear();return;}
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    if (resetLinkRuntime.exchange(false, std::memory_order_acquire)) linkRuntime = {};
    engine.setParameters(parameterReader.read(linkRuntime));
    auto main = getBusBuffer(buffer, false, 0);
    auto key = getBusBuffer(buffer, true, 1);
    if (bypassDelay[0].empty()) return;
    const int length = static_cast<int>(bypassDelay[0].size());
    for (int offset = 0; offset < main.getNumSamples();)
    {
        const int count = std::min(main.getNumSamples() - offset, static_cast<int>(bypassWork[0].size()));
        std::array<float*, 2> program {};
        std::array<const float*, 2> detector {};
        for (int channel = 0; channel < main.getNumChannels(); ++channel)
        {
            program[static_cast<std::size_t>(channel)] = bypassWork[static_cast<std::size_t>(channel)].data();
            std::copy_n(main.getReadPointer(channel, offset), count, program[static_cast<std::size_t>(channel)]);
        }
        for (int channel = 0; channel < key.getNumChannels(); ++channel) detector[static_cast<std::size_t>(channel)] = key.getReadPointer(channel, offset);
        // Keep wet history/dynamics warm so returning from bypass loses no transient.
        engine.process(program.data(), main.getNumChannels(), detector.data(), key.getNumChannels(), count);
        for (int sample = offset; sample < offset + count; ++sample)
        {
            for (int channel = 0; channel < main.getNumChannels(); ++channel)
            {
                auto& delay = bypassDelay[static_cast<std::size_t>(channel)];
                const float raw = main.getSample(channel, sample);
                delay[static_cast<std::size_t>(bypassPosition)] = std::isfinite(raw) ? raw : 0.0f;
                main.setSample(channel, sample, delay[static_cast<std::size_t>((bypassPosition + 1) % length)]);
            }
            bypassPosition = (bypassPosition + 1) % length;
        }
        offset += count;
    }
    applyReloadFade(main);
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
            // Fill every missing field from parameter defaults, never from the
            // current instance. Old presets loaded over LP/Dual Mono restore IIR/Stereo.
            for (auto* parameter : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                {
                    auto child = restored.getChildWithProperty("id", ranged->paramID);
                    const float fallback = ranged->convertFrom0to1(ranged->getDefaultValue());
                    if (!child.isValid())
                    {
                        child = juce::ValueTree("PARAM");
                        child.setProperty("id", ranged->paramID, nullptr);
                        restored.addChild(child, -1, nullptr);
                        child.setProperty("value", fallback, nullptr);
                    }
                    const double raw = static_cast<double>(child.getProperty("value", fallback));
                    const float value = std::isfinite(raw) ? static_cast<float>(raw) : fallback;
                    child.setProperty("value", ranged->convertFrom0to1(ranged->convertTo0to1(value)), nullptr);
                }
            restored.setProperty("schemaVersion", pontedsp::mc2000::parameters::stateSchemaVersion, nullptr);
            state.replaceState(restored);
            resetLinkRuntime.store(true, std::memory_order_release);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PonteMC2000AudioProcessor();
}

void PonteMC2000AudioProcessor::timerCallback()
{
    if(stopReload.load(std::memory_order_acquire) || !prepared.load(std::memory_order_acquire))return;
    // The host may prepare on a different thread. Do not overwrite its loading
    // gate with fadingOut, which would reopen coefficient/FIFO readers.
    std::unique_lock<std::mutex> configuration(configurationMutex,std::try_to_lock);
    if(!configuration.owns_lock() || !prepared.load(std::memory_order_acquire))return;
    const auto stage=getReloadState();
    if(!reloadFinished.load(std::memory_order_acquire) || (stage!=ReloadState::idle && stage!=ReloadState::fadingIn))return;
    const int requested=getRequestedCrossoverMode();
    if(requested==getActiveCrossoverMode())return;
    if(reloadThread.joinable())reloadThread.join();
    reloadSourceMode.store(getActiveCrossoverMode(),std::memory_order_relaxed);
    reloadTargetMode.store(requested,std::memory_order_relaxed);
    reloadFinished.store(false,std::memory_order_release);
    reloadState.store(ReloadState::fadingOut,std::memory_order_seq_cst);
    const auto generation=configurationGeneration.load(std::memory_order_seq_cst);
    try { reloadThread=std::thread([this,generation]{reloadCrossover(generation);}); }
    catch(...) { reloadFinished.store(true,std::memory_order_release);reloadState.store(ReloadState::failed,std::memory_order_seq_cst); }
}
void PonteMC2000AudioProcessor::reloadCrossover(const unsigned generation)
{
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
    while(getReloadState()==ReloadState::fadingOut && !stopReload.load(std::memory_order_acquire)
          && configurationGeneration.load(std::memory_order_seq_cst)==generation
          && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(stopReload.load(std::memory_order_acquire)){reloadFinished.store(true,std::memory_order_release);return;}
    std::lock_guard<std::mutex> lock(configurationMutex);
    if(configurationGeneration.load(std::memory_order_seq_cst)!=generation){reloadFinished.store(true,std::memory_order_release);return;}
    reloadState.store(ReloadState::loading,std::memory_order_seq_cst);
    preparing.store(true,std::memory_order_release);
    waitForCallbacks();
    try {
        configureEngine(getProcessingSampleRate(),preparedBlock.load(),preparedMainChannels.load());
        reloadTargetMode.store(getActiveCrossoverMode(),std::memory_order_relaxed);
        reloadState.store(ReloadState::notifyingHost,std::memory_order_seq_cst);
        triggerAsyncUpdate(); // Host notification is delivered only on the message thread.
    } catch(...) {
        reloadState.store(ReloadState::failed,std::memory_order_seq_cst);
        // Remain gated: never resume a partially prepared engine.
    }
    reloadFinished.store(true,std::memory_order_release);
}
void PonteMC2000AudioProcessor::handleAsyncUpdate()
{
    if(getReloadState()!=ReloadState::notifyingHost)return;
    // Do not hold the preparation mutex while notifying: hosts may synchronously
    // reactivate us from their listener. Keep callbacks gated until notification.
    setLatencySamples(getActiveLatencySamples());
    auto expected=ReloadState::notifyingHost;
    if(reloadState.compare_exchange_strong(expected,ReloadState::fadingIn,std::memory_order_seq_cst))
        preparing.store(false,std::memory_order_release);
}
void PonteMC2000AudioProcessor::applyReloadFade(juce::AudioBuffer<float>& buffer) noexcept
{
    const auto stage=getReloadState();
    if(stage==ReloadState::idle){lastFadeState=stage;return;}
    if(stage!=lastFadeState){
        // Keep the current gain when a new request interrupts the warm-up.
        if(stage==ReloadState::fadingIn){reloadGain=0;reloadWarmSamples=getActiveLatencySamples();}
        lastFadeState=stage;
    }
    const double step=1.0/std::max(1.0,getProcessingSampleRate()*.010);
    for(int i=0;i<buffer.getNumSamples();++i){
        if(stage==ReloadState::fadingOut)reloadGain=std::max(0.0,reloadGain-step);
        else if(stage==ReloadState::fadingIn){
            if(reloadWarmSamples>0)--reloadWarmSamples;
            else reloadGain=std::min(1.0,reloadGain+step);
        }else reloadGain=0;
        for(int c=0;c<buffer.getNumChannels();++c)buffer.setSample(c,i,float(buffer.getSample(c,i)*reloadGain));
    }
    if(stage==ReloadState::fadingOut && reloadGain==0){
        auto expected=stage;reloadState.compare_exchange_strong(expected,ReloadState::loading,std::memory_order_seq_cst);
    }else if(stage==ReloadState::fadingIn && reloadGain==1){
        auto expected=stage;reloadState.compare_exchange_strong(expected,ReloadState::idle,std::memory_order_seq_cst);
    }
}
juce::String PonteMC2000AudioProcessor::getCrossoverReloadMessage() const
{
    const auto stage=getReloadState();
    if(stage==ReloadState::idle || stage==ReloadState::fadingIn)return {};
    if(stage==ReloadState::failed)return "Crossover load failed. Reactivate the plugin.";
    const auto name=[](int mode){return mode==1?"Linear Phase":"IIR";};
    return juce::String("Loading ")+name(reloadSourceMode.load())+" -> "+name(reloadTargetMode.load())
        +(stage==ReloadState::notifyingHost?"\nUpdating host latency...":stage==ReloadState::fadingIn?"\nWarming audio...":"\nPreparing filters...");
}
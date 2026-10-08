#pragma once

#include "DSP/MultiBandCompressor.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>
#include <thread>

class PonteMC2000AudioProcessor final : public juce::AudioProcessor, private juce::Timer, private juce::AsyncUpdater
{
public:
    PonteMC2000AudioProcessor();
    ~PonteMC2000AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Conservative audio-filter tail at the minimum 20 Hz crossover.
    // Detector release alone is not an audible tail.
    double getTailLengthSeconds() const override { return activeCrossoverMode.load(std::memory_order_relaxed) == 1 ? linearTailSeconds.load(std::memory_order_relaxed) : 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int size) override;

    pontedsp::mc2000::dsp::MultiBandCompressor& getEngine() noexcept { return engine; }
    const pontedsp::mc2000::dsp::MultiBandCompressor& getEngine() const noexcept { return engine; }
    struct SpectrumSample
    {
        std::array<float, 4> audio {}; // program L/R, external key L/R
        unsigned sidechainBandMask {}; // capture-time routing; zero for unavailable/unused key
        float operator[](std::size_t index) const noexcept { return audio[index]; }
    };
    int popSpectrumSamples(SpectrumSample* destination, int maximumSamples, bool& discontinuity) noexcept;
    void discardSpectrumSamples() noexcept;
    // Message-thread ownership; the audio thread only reads this counter.
    void addSpectrumConsumer() noexcept { spectrumConsumers.fetch_add(1, std::memory_order_release); }
    void removeSpectrumConsumer() noexcept { spectrumConsumers.fetch_sub(1, std::memory_order_release); }
    int getActiveCrossoverMode() const noexcept { return activeCrossoverMode.load(std::memory_order_relaxed); }
    int getRequestedCrossoverMode() const noexcept { return static_cast<int>(pontedsp::mc2000::dsp::clampFinite(requestedCrossoverMode->load(),0.0,1.0,0.0)); }
    bool isCrossoverModePending() const noexcept { return getRequestedCrossoverMode()!=getActiveCrossoverMode(); }
    enum class ReloadState { idle, fadingOut, loading, notifyingHost, fadingIn, failed };
    ReloadState getReloadState() const noexcept { return reloadState.load(std::memory_order_seq_cst); }
    bool isCrossoverReloading() const noexcept { return getReloadState()!=ReloadState::idle; }
    juce::String getCrossoverReloadMessage() const;
    int getActiveLatencySamples() const noexcept { return activeLatency.load(std::memory_order_acquire); }
    bool isPreparing() const noexcept { return preparing.load(std::memory_order_acquire); }
    double getProcessingSampleRate() const noexcept { return processingSampleRate.load(std::memory_order_relaxed); }

    struct ResponseReadAccess {
        explicit ResponseReadAccess(PonteMC2000AudioProcessor& p) noexcept : owner(p) {
            owner.callbackReaders.fetch_add(1,std::memory_order_seq_cst);
            const auto stage=owner.getReloadState();
            allowed=stage==ReloadState::idle || stage==ReloadState::fadingOut || stage==ReloadState::fadingIn;
        }
        ~ResponseReadAccess(){owner.callbackReaders.fetch_sub(1,std::memory_order_seq_cst);}
        PonteMC2000AudioProcessor& owner;bool allowed {};
    };
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> editorWidth { 1100 }, editorHeight { 738 };

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void configureEngine(double rate,int block,int channels);
    void reloadCrossover(unsigned generation);
    void waitForCallbacks();
    void applyReloadFade(juce::AudioBuffer<float>&) noexcept;
    struct CallbackAccess {
        explicit CallbackAccess(PonteMC2000AudioProcessor& p) noexcept : owner(p) {
            owner.callbackReaders.fetch_add(1,std::memory_order_seq_cst);
            const auto stage=owner.getReloadState();
            allowed=owner.prepared.load(std::memory_order_acquire) && (stage==ReloadState::idle || stage==ReloadState::fadingOut || stage==ReloadState::fadingIn);
        }
        ~CallbackAccess(){owner.callbackReaders.fetch_sub(1,std::memory_order_seq_cst);}
        PonteMC2000AudioProcessor& owner;bool allowed {};
    };
    std::atomic<ReloadState> reloadState {ReloadState::idle};
    std::atomic<unsigned> callbackReaders {}, configurationGeneration {};
    std::atomic<bool> stopReload {}, reloadFinished {true}, prepared {false};
    std::atomic<int> preparedBlock {1}, preparedMainChannels {2}, activeLatency {}, reloadSourceMode {}, reloadTargetMode {};
    std::atomic<float>* requestedCrossoverMode {};
    std::mutex configurationMutex;
    std::thread reloadThread;
    ReloadState lastFadeState {ReloadState::idle};
    double reloadGain {1};
    int reloadWarmSamples {};
    pontedsp::mc2000::parameters::SnapshotReader parameterReader { state };
    void pushSpectrumSamples(const juce::AudioBuffer<float>& program, const juce::AudioBuffer<float>& key) noexcept;

    static constexpr int spectrumFifoCapacity = 32768;
    std::atomic<int> spectrumConsumers {};
    pontedsp::mc2000::dsp::MultiBandCompressor engine;
    pontedsp::mc2000::parameters::LinkRuntime linkRuntime;
    // Allocate once during construction, never resize in a callback.
    std::vector<SpectrumSample> spectrumSamples = std::vector<SpectrumSample>(spectrumFifoCapacity);
    std::atomic<bool> spectrumOverflow { false };
    juce::AbstractFifo spectrumFifo { spectrumFifoCapacity };
    std::atomic<int> activeCrossoverMode {};
    std::atomic<double> linearTailSeconds {};
    std::atomic<bool> resetLinkRuntime { false };
    std::array<std::vector<float>, 2> bypassDelay;
    int bypassPosition {};
    std::array<std::vector<float>, 2> bypassWork;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    std::atomic<bool> preparing {};
    std::atomic<double> processingSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PonteMC2000AudioProcessor)
};

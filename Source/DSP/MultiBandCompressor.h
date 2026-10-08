#pragma once

#include "Ballistics.h"
#include "CrossoverNetwork.h"
#include "GainComputer.h"
#include "MeterPeak.h"
#include "LinearPhaseCrossover.h"
#include <array>
#include <atomic>

namespace pontedsp::mc2000::dsp {

enum class CrossoverMode { iir = 0, linearPhase = 1 };
enum class ChannelMode { stereo = 0, dualMono = 1 };
enum class SidechainSource { internal = 0, all = 1 };

struct BandParameters
{
    bool operator==(const BandParameters&) const = default;
    bool enabled { true }; // IN: admit this band's input, otherwise mute it.
    bool solo { false };
    double gainDb {};
    double thresholdDb {};
    double ratio { 1.0 };
    double knee {};
    double bite { 1.0 };
    double attackMs { 10.0 };
    double releaseMs { 250.0 };
    TCMode tcMode { TCMode::type1 };
    SidechainSource sidechainSource { SidechainSource::internal };
};

struct GlobalParameters
{
    bool operator==(const GlobalParameters&) const = default;
    double inputGainDb {};
    double outputGainDb {};
    bool phaseInvert {};
    int numBands { 4 };
    std::array<double, 3> crossoverHz { 100.0, 1000.0, 10000.0 };
    CrossoverMode crossoverMode { CrossoverMode::iir };
    ChannelMode channelMode { ChannelMode::stereo };
    std::array<BandParameters, 4> bands {};
};

struct BandMeterSnapshot
{
    bool operator==(const BandMeterSnapshot&) const = default;
    float inputDb { -100.0f };
    float outputDb { -100.0f };
    float gainReductionDb {};
    std::array<float, 2> channelGainReductionDb {}; // independent block peaks; aggregate is max(L,R)
    std::array<float, 2> channelInputDb { -100.0f, -100.0f };
    std::array<float, 2> channelOutputDb { -100.0f, -100.0f };
};

class MultiBandCompressor final
{
public:
    static constexpr int maxBands = CrossoverNetwork::maxBands;
    static constexpr int maxChannels = CrossoverNetwork::maxChannels;
    static constexpr int dspModelVersion = 11;

    enum class Activity { active, draining, sleeping };
    // Audio-thread only. Disabling nap is used by the equivalence harness.
    void setNapEnabled(bool enabled) noexcept { napEnabled = enabled; activity = Activity::active; }
    Activity getActivity() const noexcept { return activity; }

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;
    int getLatencySamples() const noexcept { return activeCrossoverMode == CrossoverMode::linearPhase ? linearCrossover.latencySamples() : 0; }
    CrossoverMode getActiveCrossoverMode() const noexcept { return activeCrossoverMode; }
    unsigned getLinearResponseVersion() const noexcept { return linearCrossover.getResponseVersion(); }
    unsigned getLinearSchedulingOverruns() const noexcept { return linearCrossover.getSchedulingOverruns(); }
    bool isLinearTransitioning() const noexcept { return linearCrossover.isTransitioning(); }
    bool copyLinearResponse(LinearPhaseCrossover::Responses& r, std::array<double, 3>& f, unsigned& v) const noexcept { return linearCrossover.copyResponse(r, f, v); }
    bool copyIirResponse(CrossoverResponse& response, unsigned& version) const noexcept;
    void setParameters(const GlobalParameters& parameters) noexcept;
    void process(float** channels, int numChannels, int numSamples) noexcept;
    // The external key is the host's audio sum of all sidechain sends.
    // Each band chooses internal (NO) or external (ALL) detection independently.
    // A missing ALL key is silence; it never falls back to program detection.
    void process(float** channels, int numChannels,
                 const float* const* detectorChannels, int detectorNumChannels,
                 int numSamples) noexcept;

    double getStaticOutputDb(int band, double inputDb) const noexcept;
    std::array<double, 3> getStaticCurveParameters(int band) const noexcept;
    double getBandMagnitudeDb(int band, double frequency) const noexcept;
    BandMeterSnapshot getBandMeter(int band) const noexcept;
    std::array<float, 2> getOutputMeterDb() const noexcept;
    // Consume once per GUI tick. Raw getters above remain per-block readings
    // for analysis; painting must use cached, smoothed GUI values.
    BandMeterSnapshot consumeBandMeter(int band) noexcept;
    std::array<float, 2> consumeOutputMeterDb() noexcept;
    void discardPendingMeterPeaks() noexcept;
    const GlobalParameters& getParameters() const noexcept { return currentParameters; }

private:
    void publishIirResponse() noexcept;
    std::atomic<unsigned> iirResponseVersion {};
    std::array<std::atomic<double>,124> iirResponseData {};
    std::atomic<int> iirResponseBands {4};
    unsigned lastIirRevision {~0u};
    struct AtomicCurve
    {
        std::atomic<double> threshold {}, ratio { 1.0 }, knee {};
    };
    std::array<AtomicCurve, maxBands> curves;

    struct AtomicBandMeter
    {
        std::atomic<float> inputDb { -100.0f };
        std::atomic<float> outputDb { -100.0f };
        std::atomic<float> gainReductionDb {};
        std::array<std::atomic<float>, 2> channelInputDb { -100.0f, -100.0f };
        std::array<std::atomic<float>, 2> channelOutputDb { -100.0f, -100.0f };
    };

    static double smoothGain(double current, double target, double coefficient) noexcept;
    void publishMeters(const std::array<double, maxBands>& inputPeaks,
                       const std::array<double, maxBands>& outputPeaks,
                       const std::array<double, maxBands>& maximumGr,
                       const std::array<double, 2>& outputPeaksMaster,
                       const std::array<std::array<double, maxBands>, maxChannels>& channelInputPeaks,
                       const std::array<std::array<double, maxBands>, maxChannels>& channelOutputPeaks) noexcept;

    LinearPhaseCrossover linearCrossover;
    CrossoverMode activeCrossoverMode { CrossoverMode::iir };
    std::array<std::array<double, maxBands>, maxChannels> lastAppliedGr {}, transitionFromGr {};
    int channelTransitionRemaining {}, channelTransitionLength {};
    std::array<std::array<std::atomic<float>, maxBands>, maxChannels> channelGr {};
    struct PendingChannelReduction { MeterPeak value {0.0f}; };
    std::array<std::array<PendingChannelReduction, maxBands>, maxChannels> pendingChannelGr;
    CrossoverNetwork crossover;
    CrossoverNetwork detectorCrossover;
    GainComputer gainComputer;
    std::array<std::array<Ballistics, maxBands>, maxChannels> ballistics;
    std::array<std::array<BiteProcessor, maxBands>, maxChannels> biteProcessors;
    std::array<AtomicBandMeter, maxBands> meters;
    std::array<std::atomic<float>, 2> outputMeters { -100.0f, -100.0f };
    struct PendingBandMeter
    {
        MeterPeak input, output, reduction { 0.0f };
        std::array<MeterPeak, 2> channelInput, channelOutput;
    };
    std::array<PendingBandMeter, maxBands> pendingMeters;
    std::array<MeterPeak, 2> pendingOutputMeters;
    GlobalParameters currentParameters;
    double sampleRate { 48000.0 };
    double gainSmoothing {}, routeSmoothing {};
    double inputGainTarget { 1.0 }, outputGainTarget { 1.0 };
    std::array<double, maxBands> bandGainTargets { 1.0, 1.0, 1.0, 1.0 };
    double inputGainCurrent { 1.0 };
    double outputGainCurrent { 1.0 };
    std::array<double, maxBands> bandGainCurrent { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, maxBands> inputMixCurrent { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, maxBands> soloMixCurrent { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 3> crossoverCurrent { 100.0, 1000.0, 10000.0 };
    int crossoverUpdateCountdown {};
    int preparedBlockSize {};
    int preparedChannels { 2 };
    bool napEnabled { true };
    Activity activity { Activity::active };
    int previousAudioChannels {}, previousDetectorChannels {};
};

} // namespace pontedsp::mc2000::dsp

#pragma once

#include "Ballistics.h"
#include "CrossoverNetwork.h"
#include "GainComputer.h"
#include <array>
#include <atomic>

namespace pontedsp::mc2000::dsp {

struct BandParameters
{
    bool enabled { true };
    bool solo { false };
    double gainDb {};
    double thresholdDb {};
    double ratio { 1.0 };
    double knee {};
    double bite { 1.0 };
    double attackMs { 10.0 };
    double releaseMs { 250.0 };
    TCMode tcMode { TCMode::type1 };
};

struct GlobalParameters
{
    double inputGainDb {};
    double outputGainDb {};
    bool phaseInvert {};
    int numBands { 4 };
    std::array<double, 3> crossoverHz { 100.0, 1000.0, 10000.0 };
    std::array<BandParameters, 4> bands {};
};

struct BandMeterSnapshot
{
    float inputDb { -100.0f };
    float outputDb { -100.0f };
    float gainReductionDb {};
};

class MultiBandCompressor final
{
public:
    static constexpr int maxBands = CrossoverNetwork::maxBands;
    static constexpr int maxChannels = CrossoverNetwork::maxChannels;
    static constexpr int dspModelVersion = 2;

    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;
    void setParameters(const GlobalParameters& parameters) noexcept;
    void process(float** channels, int numChannels, int numSamples) noexcept;

    double getStaticOutputDb(int band, double inputDb) const noexcept;
    double getBandMagnitudeDb(int band, double frequency) const noexcept;
    BandMeterSnapshot getBandMeter(int band) const noexcept;
    std::array<float, 2> getOutputMeterDb() const noexcept;
    const GlobalParameters& getParameters() const noexcept { return currentParameters; }

private:
    struct AtomicBandMeter
    {
        std::atomic<float> inputDb { -100.0f };
        std::atomic<float> outputDb { -100.0f };
        std::atomic<float> gainReductionDb {};
    };

    static double smoothGain(double current, double target, double coefficient) noexcept;
    void publishMeters(const std::array<double, maxBands>& inputPeaks,
                       const std::array<double, maxBands>& outputPeaks,
                       const std::array<double, maxBands>& maximumGr,
                       const std::array<double, 2>& outputPeaksMaster) noexcept;

    CrossoverNetwork crossover;
    GainComputer gainComputer;
    std::array<Ballistics, maxBands> ballistics;
    std::array<BiteProcessor, maxBands> biteProcessors;
    std::array<AtomicBandMeter, maxBands> meters;
    std::array<std::atomic<float>, 2> outputMeters { -100.0f, -100.0f };
    GlobalParameters currentParameters;
    double sampleRate { 48000.0 };
    double inputGainCurrent { 1.0 };
    double outputGainCurrent { 1.0 };
    std::array<double, maxBands> bandGainCurrent { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, maxBands> enabledMixCurrent { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 3> crossoverCurrent { 100.0, 1000.0, 10000.0 };
    int preparedBlockSize {};
    int preparedChannels { 2 };
};

} // namespace pontedsp::mc2000::dsp

#pragma once

#include "LinkwitzRiley4.h"
#include <array>

namespace pontedsp::mc2000::dsp {

class CrossoverNetwork final
{
public:
    static constexpr int maxBands = 4;
    static constexpr int maxChannels = 2;

    void prepare(double sampleRate, int channels) noexcept;
    void reset() noexcept;
    void setBandCount(int count) noexcept;
    void setFrequencies(const std::array<double, 3>& frequencies) noexcept;
    void processSample(int channel, double input, std::array<double, maxBands>& bands) noexcept;

    static double lowPassMagnitude(double frequency, double crossover) noexcept;
    static double highPassMagnitude(double frequency, double crossover) noexcept;
    double getBandMagnitudeDb(int band, double frequency) const noexcept;

private:
    struct ChannelFilters
    {
        std::array<LinkwitzRiley4, 3> split;
        std::array<LinkwitzRiley4, 3> compensation;
    };

    void updateCoefficients(bool resetState) noexcept;

    std::array<ChannelFilters, maxChannels> filters;
    std::array<double, 3> crossoverHz { 100.0, 1000.0, 10000.0 };
    double sampleRate { 48000.0 };
    int numChannels { 2 };
    int numBands { 4 };
};

} // namespace pontedsp::mc2000::dsp

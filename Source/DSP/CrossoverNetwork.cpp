#include "CrossoverNetwork.h"
#include "Db.h"

namespace pontedsp::mc2000::dsp {

void CrossoverNetwork::prepare(const double newSampleRate, const int channels) noexcept
{
    sampleRate = std::max(1.0, newSampleRate);
    numChannels = std::clamp(channels, 1, maxChannels);
    updateCoefficients(true);
}

void CrossoverNetwork::reset() noexcept
{
    for (auto& channel : filters)
    {
        for (auto& split : channel.split) split.reset();
        for (auto& compensation : channel.compensation) compensation.reset();
    }
}

void CrossoverNetwork::setBandCount(const int count) noexcept
{
    numBands = std::clamp(count, 2, maxBands);
}

void CrossoverNetwork::setFrequencies(const std::array<double, 3>& frequencies) noexcept
{
    auto next = frequencies;
    next[0] = std::clamp(next[0], 20.0, 18000.0);
    next[1] = std::clamp(next[1], next[0] + 1.0, 19000.0);
    next[2] = std::clamp(next[2], next[1] + 1.0, 20000.0);
    if (next == crossoverHz)
        return;
    crossoverHz = next;
    updateCoefficients(false);
}

void CrossoverNetwork::updateCoefficients(const bool resetState) noexcept
{
    for (auto& channel : filters)
    {
        for (int i = 0; i < 3; ++i)
        {
            auto& split = channel.split[static_cast<std::size_t>(i)];
            if (resetState) split.prepare(sampleRate, crossoverHz[static_cast<std::size_t>(i)]);
            else split.setFrequency(crossoverHz[static_cast<std::size_t>(i)]);
        }

        if (resetState)
        {
            channel.compensation[0].prepare(sampleRate, crossoverHz[0]);
            channel.compensation[1].prepare(sampleRate, crossoverHz[0]);
            channel.compensation[2].prepare(sampleRate, crossoverHz[1]);
        }
        else
        {
            channel.compensation[0].setFrequency(crossoverHz[0]);
            channel.compensation[1].setFrequency(crossoverHz[0]);
            channel.compensation[2].setFrequency(crossoverHz[1]);
        }
    }
}

void CrossoverNetwork::processSample(const int channelIndex, const double input,
                                     std::array<double, maxBands>& bands) noexcept
{
    bands.fill(0.0);
    auto& f = filters[static_cast<std::size_t>(std::clamp(channelIndex, 0, numChannels - 1))];

    if (numBands == 2)
    {
        const auto [low, high] = f.split[0].split(input);
        bands[0] = low;
        bands[1] = high;
        return;
    }

    if (numBands == 3)
    {
        const auto [lower, high] = f.split[1].split(input);
        const auto [low, mid] = f.split[0].split(lower);
        bands[0] = low;
        bands[1] = mid;
        bands[2] = f.compensation[0].processAllPass(high);
        return;
    }

    const auto [lowerThree, high] = f.split[2].split(input);
    const auto [lowerTwo, upperMid] = f.split[1].split(lowerThree);
    const auto [low, lowerMid] = f.split[0].split(lowerTwo);
    bands[0] = low;
    bands[1] = lowerMid;
    bands[2] = f.compensation[0].processAllPass(upperMid);
    bands[3] = f.compensation[2].processAllPass(
        f.compensation[1].processAllPass(high));
}

double CrossoverNetwork::lowPassMagnitude(const double frequency, const double crossover) noexcept
{
    const auto ratio = frequency / std::max(1.0, crossover);
    return 1.0 / (1.0 + std::pow(ratio, 4.0));
}

double CrossoverNetwork::highPassMagnitude(const double frequency, const double crossover) noexcept
{
    const auto ratio = std::max(1.0, crossover) / std::max(1.0, frequency);
    return 1.0 / (1.0 + std::pow(ratio, 4.0));
}

double CrossoverNetwork::getBandMagnitudeDb(const int band, const double frequency) const noexcept
{
    if (band < 0 || band >= numBands)
        return -160.0;

    double magnitude = 1.0;
    if (numBands == 2)
        magnitude = band == 0 ? lowPassMagnitude(frequency, crossoverHz[0])
                              : highPassMagnitude(frequency, crossoverHz[0]);
    else if (numBands == 3)
    {
        if (band == 0) magnitude = lowPassMagnitude(frequency, crossoverHz[0]) * lowPassMagnitude(frequency, crossoverHz[1]);
        if (band == 1) magnitude = highPassMagnitude(frequency, crossoverHz[0]) * lowPassMagnitude(frequency, crossoverHz[1]);
        if (band == 2) magnitude = highPassMagnitude(frequency, crossoverHz[1]);
    }
    else
    {
        if (band == 0) magnitude = lowPassMagnitude(frequency, crossoverHz[0]) * lowPassMagnitude(frequency, crossoverHz[1]) * lowPassMagnitude(frequency, crossoverHz[2]);
        if (band == 1) magnitude = highPassMagnitude(frequency, crossoverHz[0]) * lowPassMagnitude(frequency, crossoverHz[1]) * lowPassMagnitude(frequency, crossoverHz[2]);
        if (band == 2) magnitude = highPassMagnitude(frequency, crossoverHz[1]) * lowPassMagnitude(frequency, crossoverHz[2]);
        if (band == 3) magnitude = highPassMagnitude(frequency, crossoverHz[2]);
    }
    return gainToDecibels(magnitude);
}

} // namespace pontedsp::mc2000::dsp

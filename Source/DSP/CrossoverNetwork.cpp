#include "CrossoverNetwork.h"
#include "Db.h"

namespace pontedsp::mc2000::dsp {

void CrossoverNetwork::prepare(const double newSampleRate, const int channels) noexcept
{
    sampleRate = clampFinite(newSampleRate, 8000.0, 384000.0, 48000.0);
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

bool CrossoverNetwork::isQuiet(const int activeChannels) const noexcept
{
    for (int i = 0; i < std::min(activeChannels, numChannels); ++i)
    {
        const auto& f = filters[static_cast<std::size_t>(i)];
        for (int j = 0; j < numBands - 1; ++j)
            if (!f.split[static_cast<std::size_t>(j)].isQuiet()) return false;
        if (numBands >= 3 && !f.compensation[0].isQuiet()) return false;
        if (numBands == 4 && (!f.compensation[1].isQuiet() || !f.compensation[2].isQuiet())) return false;
    }
    return true;
}

void CrossoverNetwork::setBandCount(const int count) noexcept
{
    const int next = std::clamp(count, 2, maxBands);
    if (next != numBands) { numBands = next; ++revision; }
}

void CrossoverNetwork::setFrequencies(const std::array<double, 3>& frequencies) noexcept
{
    const auto next = effectiveFrequencies(frequencies, sampleRate);
    if (next == crossoverHz)
        return;
    crossoverHz = next;
    updateCoefficients(false);
}

std::array<double, 3> CrossoverNetwork::effectiveFrequencies(const std::array<double, 3>& frequencies, double rate) noexcept
{
    auto f = frequencies;
    f[0] = clampFinite(f[0], 20.0, 18000.0, 100.0);
    f[1] = clampFinite(f[1], f[0] + 1.0, 19000.0, 1000.0);
    f[2] = clampFinite(f[2], f[1] + 1.0, 20000.0, 10000.0);
    const double ceiling = .45 * clampFinite(rate, 8000.0, 384000.0, 48000.0);
    for (auto& value : f) value = std::min(value, ceiling);
    return f;
}

void CrossoverNetwork::updateCoefficients(const bool resetState) noexcept
{
    ++revision;
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
    const auto response = getBandResponse(band, frequency);
    return gainToDecibels(std::abs(response), -160.0);
}

CrossoverResponse CrossoverNetwork::responseSnapshot() const noexcept
{
    CrossoverResponse result;result.sampleRate=sampleRate;result.bands=numBands;
    result.frequencies=crossoverHz;
    for(std::size_t i=0;i<3;++i){result.sections[i]=filters[0].split[i].coefficients();result.sections[i+3]=filters[0].compensation[i].coefficients();}
    return result;
}
std::complex<double> CrossoverNetwork::getBandResponse(int band,double frequency) const noexcept
{ return responseSnapshot().bandResponse(band,frequency); }
} // namespace pontedsp::mc2000::dsp
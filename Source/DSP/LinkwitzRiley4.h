#pragma once

#include "Biquad.h"
#include <algorithm>
#include <utility>

namespace pontedsp::mc2000::dsp {

class LinkwitzRiley4 final
{
public:
    void prepare(const double newSampleRate, const double frequency) noexcept
    {
        sampleRate = newSampleRate;
        setFrequency(frequency);
        reset();
    }

    void setFrequency(const double frequency) noexcept
    {
        const auto safe = std::clamp(frequency, 20.0, sampleRate * 0.45);
        low1.configure(Biquad::Type::lowPass, sampleRate, safe);
        low2.configure(Biquad::Type::lowPass, sampleRate, safe);
        high1.configure(Biquad::Type::highPass, sampleRate, safe);
        high2.configure(Biquad::Type::highPass, sampleRate, safe);
    }

    std::pair<double, double> split(const double input) noexcept
    {
        return { low2.process(low1.process(input)), high2.process(high1.process(input)) };
    }

    double processAllPass(const double input) noexcept
    {
        const auto [low, high] = split(input);
        return low + high;
    }

    void reset() noexcept
    {
        low1.reset(); low2.reset(); high1.reset(); high2.reset();
    }

private:
    double sampleRate { 48000.0 };
    Biquad low1, low2, high1, high2;
};

} // namespace pontedsp::mc2000::dsp

#pragma once

#include "Biquad.h"
#include "Db.h"
#include <algorithm>
#include <utility>
#include <array>
#include <complex>

namespace pontedsp::mc2000::dsp {

class LinkwitzRiley4 final
{
public:
    void prepare(const double newSampleRate, const double frequency) noexcept
    {
        sampleRate = clampFinite(newSampleRate, 8000.0, 384000.0, 48000.0);
        cachedFrequency = -1.0;
        setFrequency(frequency);
        reset();
    }

    void setFrequency(const double frequency) noexcept
    {
        const auto safe = clampFinite(frequency, 20.0, sampleRate * 0.45, 100.0);
        if (exactlyEqual(safe, cachedFrequency)) return;
        cachedFrequency = safe;
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

    std::complex<double> lowPassResponse(const double frequency) const noexcept
    {
        return low1.response(frequency, sampleRate) * low2.response(frequency, sampleRate);
    }

    std::complex<double> highPassResponse(const double frequency) const noexcept
    {
        return high1.response(frequency, sampleRate) * high2.response(frequency, sampleRate);
    }

    std::complex<double> allPassResponse(const double frequency) const noexcept
    {
        return lowPassResponse(frequency) + highPassResponse(frequency);
    }

    std::array<Biquad::Coefficients,4> coefficients() const noexcept
    { return {low1.coefficients(),low2.coefficients(),high1.coefficients(),high2.coefficients()}; }
    bool isQuiet() const noexcept
    { return low1.isQuiet() && low2.isQuiet() && high1.isQuiet() && high2.isQuiet(); }

    void reset() noexcept
    {
        low1.reset(); low2.reset(); high1.reset(); high2.reset();
    }

private:
    double sampleRate { 48000.0 };
    double cachedFrequency { -1.0 };
    Biquad low1, low2, high1, high2;
};

} // namespace pontedsp::mc2000::dsp

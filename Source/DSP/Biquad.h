#pragma once

#include <cmath>

namespace pontedsp::mc2000::dsp {

class Biquad final
{
public:
    enum class Type { lowPass, highPass };

    void configure(const Type type, const double sampleRate, const double frequency) noexcept
    {
        constexpr double q = 0.70710678118654752440;
        const auto k = std::tan(3.14159265358979323846 * frequency / sampleRate);
        const auto norm = 1.0 / (1.0 + k / q + k * k);

        if (type == Type::lowPass)
        {
            b0 = k * k * norm;
            b1 = 2.0 * b0;
            b2 = b0;
        }
        else
        {
            b0 = norm;
            b1 = -2.0 * norm;
            b2 = norm;
        }

        a1 = 2.0 * (k * k - 1.0) * norm;
        a2 = (1.0 - k / q + k * k) * norm;
    }

    double process(const double input) noexcept
    {
        const auto output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

private:
    double b0 { 1.0 }, b1 {}, b2 {}, a1 {}, a2 {};
    double z1 {}, z2 {};
};

} // namespace pontedsp::mc2000::dsp


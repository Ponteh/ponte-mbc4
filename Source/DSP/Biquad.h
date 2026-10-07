#pragma once

#include <cmath>
#include <complex>

namespace pontedsp::mc2000::dsp {

class Biquad final
{
public:
    enum class Type { lowPass, highPass };

    struct Coefficients { double b0 {1}, b1 {}, b2 {}, a1 {}, a2 {}; };
    static Coefficients designCoefficients(const Type type, const double sampleRate, const double frequency) noexcept
    {
        Coefficients result;
        auto& b0 = result.b0; auto& b1 = result.b1; auto& b2 = result.b2;
        auto& a1 = result.a1; auto& a2 = result.a2;
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
        return result;
    }

    void configure(const Type type, const double sampleRate, const double frequency) noexcept
    {
        const auto c = designCoefficients(type, sampleRate, frequency);
        b0 = c.b0; b1 = c.b1; b2 = c.b2; a1 = c.a1; a2 = c.a2;
    }

    double process(const double input) noexcept
    {
        const auto output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }

    bool isQuiet() const noexcept { return std::abs(z1) < 1.0e-24 && std::abs(z2) < 1.0e-24; }

    void reset() noexcept { z1 = z2 = 0.0; }

    std::complex<double> response(const double frequency, const double rate) const noexcept
    {
        const auto omega = 2.0 * 3.14159265358979323846 * frequency / rate;
        const std::complex<double> z1v = std::polar(1.0, -omega);
        const auto z2v = z1v * z1v;
        return (b0 + b1 * z1v + b2 * z2v)
             / (1.0 + a1 * z1v + a2 * z2v);
    }

private:
    double b0 { 1.0 }, b1 {}, b2 {}, a1 {}, a2 {};
    double z1 {}, z2 {};
};

} // namespace pontedsp::mc2000::dsp

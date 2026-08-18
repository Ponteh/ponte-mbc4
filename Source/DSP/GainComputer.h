#pragma once

#include "Db.h"
#include <algorithm>
#include <array>

namespace pontedsp::mc2000::dsp {

class GainComputer final
{
public:
    double computeOutputDb(const double inputDb, const double thresholdDb,
                           const double ratioValue, const double kneeValue) const noexcept
    {
        const auto input = std::isfinite(inputDb) ? inputDb : -160.0;
        const auto threshold = clampFinite(thresholdDb, -45.0, 0.0, 0.0);
        const auto ratio = clampFinite(ratioValue, 1.0, 10.0, 1.0);
        const auto knee = clampFinite(kneeValue, -10.0, 15.0, 0.0);
        const auto over = input - threshold;
        const auto hard = over <= 0.0 ? input : threshold + over / ratio;
        const auto ratioScale = (1.0 - 1.0 / ratio) / 0.75;

        if (ratioScale <= 0.0 || knee == 0.0)
            return hard;

        const auto correction = knee < 0.0 ? negativeKnee(over, -knee)
                                           : positiveKnee(over, knee);
        return hard + correction * ratioScale;
    }

    double computeGainReductionDb(const double inputDb, const double thresholdDb,
                                  const double ratio, const double knee) const noexcept
    {
        return std::max(0.0, inputDb - computeOutputDb(inputDb, thresholdDb, ratio, knee));
    }

private:
    template <std::size_t Size>
    static double pchip(const std::array<double, Size>& x,
                        const std::array<double, Size>& y,
                        const double query) noexcept
    {
        static_assert(Size >= 2);
        if (query <= x.front()) return y.front();
        if (query >= x.back()) return y.back();

        const auto upper = std::upper_bound(x.begin(), x.end(), query);
        const auto index = static_cast<std::size_t>(std::distance(x.begin(), upper) - 1);
        const auto h = x[index + 1] - x[index];
        const auto t = (query - x[index]) / h;
        const auto secant = [&](const std::size_t i)
        {
            return (y[i + 1] - y[i]) / (x[i + 1] - x[i]);
        };
        const auto slope = [&](const std::size_t i)
        {
            if (i == 0) return secant(0);
            if (i + 1 == Size) return secant(Size - 2);
            const auto before = secant(i - 1);
            const auto after = secant(i);
            if (before == 0.0 || after == 0.0 || before * after <= 0.0) return 0.0;
            const auto hBefore = x[i] - x[i - 1];
            const auto hAfter = x[i + 1] - x[i];
            const auto weight1 = 2.0 * hAfter + hBefore;
            const auto weight2 = hAfter + 2.0 * hBefore;
            return (weight1 + weight2) / (weight1 / before + weight2 / after);
        };

        const auto t2 = t * t;
        const auto t3 = t2 * t;
        return (2.0 * t3 - 3.0 * t2 + 1.0) * y[index]
             + (t3 - 2.0 * t2 + t) * h * slope(index)
             + (-2.0 * t3 + 3.0 * t2) * y[index + 1]
             + (t3 - t2) * h * slope(index + 1);
    }

    static double negativeKneeCurve5(const double over) noexcept
    {
        constexpr std::array x {-6.0,-4.0,-2.0,0.0,2.0,4.0,6.0,8.0,10.0,12.0};
        constexpr std::array y {0.0,0.738409,1.584680,2.514705,2.119791,
                                1.752519,1.437300,1.171111,0.949411,0.766368};
        if (over <= x.front()) return 0.0;
        if (over >= x.back()) return y.back() * std::exp(-(over - x.back()) / 9.2);
        return pchip(x, y, over);
    }

    static double negativeKneeCurve10(const double over) noexcept
    {
        constexpr std::array x {-12.0,-10.0,-8.0,-6.0,-4.0,-2.0,0.0,
                                2.0,4.0,6.0,8.0,10.0,12.0};
        constexpr std::array y {1.381731,1.701807,2.083093,2.533852,3.059774,
                                3.667143,4.356975,3.728193,3.139069,2.618620,
                                2.166955,1.780929,1.454819};
        if (over <= x.front()) return y.front() * std::exp((over - x.front()) / 9.5);
        if (over >= x.back()) return y.back() * std::exp(-(over - x.back()) / 10.0);
        return pchip(x, y, over);
    }

    static double negativeKnee(const double over, const double amount) noexcept
    {
        const auto curve5 = negativeKneeCurve5(over);
        if (amount <= 5.0) return -curve5 * amount / 5.0;
        const auto curve10 = negativeKneeCurve10(over);
        return -(curve5 + (curve10 - curve5) * (amount - 5.0) / 5.0);
    }

    static constexpr std::array positiveX {
        0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,
        14.0,16.0,18.0,21.0
    };

    static double positiveHump5(const double over) noexcept
    {
        constexpr std::array y {
            0.0,0.491904,0.842278,1.088481,1.247601,1.335040,1.363561,
            1.344854,1.289404,1.206479,1.104175,0.989357,0.867864,
            0.623492,0.400871,0.220005,0.051016
        };
        if (over <= 0.0) return 0.0;
        if (over >= positiveX.back()) return y.back() * std::exp(-(over - positiveX.back()) / 2.1);
        return pchip(positiveX, y, over);
    }

    static double positiveHump10(const double over) noexcept
    {
        constexpr std::array y {
            0.0,0.552771,0.999682,1.351818,1.613221,1.790709,1.891378,
            1.923875,1.897756,1.823036,1.709778,1.567527,1.405160,
            1.051326,0.702377,0.400093,0.097833
        };
        if (over <= 0.0) return 0.0;
        if (over >= positiveX.back()) return y.back() * std::exp(-(over - positiveX.back()) / 2.1);
        return pchip(positiveX, y, over);
    }

    static double positiveTailPerKneeUnit(const double over) noexcept
    {
        constexpr std::array y {
            0.0,0.002198,0.008106,0.017095,0.028628,0.042166,0.057253,
            0.073583,0.091040,0.109752,0.130097,0.152701,0.178404,
            0.243497,0.335995,0.469485,0.785490
        };
        if (over <= 0.0) return 0.0;
        if (over >= positiveX.back())
        {
            constexpr double lastSlope = (0.785490 - 0.469485) / 3.0;
            return y.back() + lastSlope * (over - positiveX.back());
        }
        return pchip(positiveX, y, over);
    }

    static double positiveKnee(const double over, const double amount) noexcept
    {
        if (over <= 0.0) return 0.0;
        const auto curve5 = positiveHump5(over);
        if (amount <= 5.0) return curve5 * amount / 5.0;
        const auto curve10 = positiveHump10(over);
        if (amount <= 10.0)
            return curve5 + (curve10 - curve5) * (amount - 5.0) / 5.0;
        return curve10 + positiveTailPerKneeUnit(over) * (amount - 10.0);
    }
};

} // namespace pontedsp::mc2000::dsp

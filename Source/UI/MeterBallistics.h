#pragma once

#include <algorithm>
#include <cmath>

namespace pontedsp::gui {

// Message-thread state; values are dB, dt is elapsed wall-clock seconds.
// MC2000 profile from Research/GUI_METER_TEST_PACK, 2026-09-15.
class LevelMeterBallistics final
{
public:
    static constexpr double floorDb = -100.0;
    static constexpr double fallDbPerSecond = 14.3;
    static constexpr double smoothingSeconds = 0.130;

    double update(double peakDb, double dt) noexcept
    {
        if (!std::isfinite(dt) || dt < 0.0) return displayed;
        const auto target = std::isfinite(peakDb) ? std::max(floorDb, peakDb) : floorDb;
        // New maxima attack immediately, including short peaks accumulated
        // between GUI ticks. No deliberate delay or peak-hold plateau.
        ramp = std::max(ramp, target);
        if (target >= displayed) return displayed = ramp = target;

        // Exact integration of a falling dB ramp followed by a one-pole.
        // Split at its arrival at target; independent of timer jitter.
        const auto fallingTime = std::min(dt, (ramp - target) / fallDbPerSecond);
        const auto endRamp = ramp - fallDbPerSecond * fallingTime;
        displayed = endRamp + fallDbPerSecond * smoothingSeconds
            + (displayed - ramp - fallDbPerSecond * smoothingSeconds)
                * std::exp(-fallingTime / smoothingSeconds);
        ramp = endRamp;
        displayed = target + (displayed - target)
            * std::exp(-(dt - fallingTime) / smoothingSeconds);
        return displayed;
    }
    double value() const noexcept { return displayed; }
    void reset() noexcept { ramp = displayed = floorDb; }

private:
    double ramp { floorDb }, displayed { floorDb };
};

class GainReductionMeterBallistics final
{
public:
    // Engineering fit to normalized R1/R250 returns (03, checked on 04).
    // This is a display constant, not a compressor release or a dB offset.
    static constexpr double smoothingSeconds = 0.150;
    double update(double peakDb, double dt) noexcept
    {
        if (!std::isfinite(dt) || dt < 0.0) return displayed;
        const auto target = std::isfinite(peakDb) ? std::max(0.0, peakDb) : 0.0;
        displayed = target >= displayed ? target : target + (displayed - target)
            * std::exp(-dt / smoothingSeconds);
        return displayed;
    }
    double value() const noexcept { return displayed; }
    void reset() noexcept { displayed = 0.0; }

private:
    double displayed {};
};

} // namespace pontedsp::gui

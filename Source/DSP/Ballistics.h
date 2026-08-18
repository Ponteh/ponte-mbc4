#pragma once

#include "Db.h"

namespace pontedsp::mc2000::dsp {

enum class TCMode { type1 = 0, type2 = 1, automatic = 2 };

class Ballistics final
{
public:
    void prepare(const double newSampleRate) noexcept
    {
        sampleRate = std::max(1.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        gainReductionDb = releaseStartDb = releaseAgeSeconds = 0.0;
        detectorEnvelope = releaseStartDetector = 0.0;
        releasing = false;
        rmsSquared = peakEnvelope = 0.0;
        previousMode = TCMode::type1;
    }

    double process(const double targetDb, const double detectorLinear,
                   const double attackMs, const double releaseMs, const TCMode mode) noexcept
    {
        const auto target = clampFinite(targetDb, 0.0, 160.0, 0.0);
        const auto detector = clampFinite(detectorLinear, 0.0, 1.0e6, 0.0);

        if (mode == TCMode::automatic)
        {
            previousMode = mode;
            releasing = false;
            return processAuto(target, detector);
        }

        const auto displayedAttackMs = clampFinite(attackMs, 0.03, 250.0, 10.0);
        // T08 fit: the displayed value is not the one-pole 63.2% time constant.
        const auto attackScale = 0.51 / (1.0 + displayedAttackMs / 800.0);
        const auto attackSeconds = displayedAttackMs * attackScale * 0.001;
        const auto releaseSeconds = clampFinite(releaseMs, 5.0, 2500.0, 250.0) * 0.001;
        const auto attackCoefficient = std::exp(-1.0 / (sampleRate * attackSeconds));

        if (mode != previousMode)
        {
            beginRelease(detector);
            previousMode = mode;
        }

        if (target > gainReductionDb)
        {
            gainReductionDb = attackCoefficient * gainReductionDb
                            + (1.0 - attackCoefficient) * target;
            detectorEnvelope = std::max(detectorEnvelope, detector);
            releasing = false;
            return gainReductionDb;
        }

        if (!releasing)
            beginRelease(detector);

        auto ageStep = 1.0 / sampleRate;
        auto effectiveRelease = releaseSeconds;
        if (mode == TCMode::type2)
        {
            // In this stretched-exponential implementation 0.838 reproduces the
            // measured ~0.77 equivalent one-pole release reported by T10/T11.
            effectiveRelease *= 0.838;
            const auto releaseFactor = stretchedRelease(releaseAgeSeconds, effectiveRelease);
            const auto envelope = releaseStartDetector * releaseFactor;
            const auto eventRatio = std::clamp(detector / std::max(envelope, 1.0e-12), 0.0, 1.0);
            ageStep *= 1.0 - 0.58 * eventRatio;
        }

        releaseAgeSeconds += ageStep;
        const auto releaseFactor = stretchedRelease(releaseAgeSeconds, effectiveRelease);
        gainReductionDb = releaseStartDb * releaseFactor;
        detectorEnvelope = releaseStartDetector * releaseFactor;
        if (gainReductionDb < 1.0e-8)
        {
            gainReductionDb = 0.0;
            detectorEnvelope = 0.0;
        }
        return gainReductionDb;
    }

private:
    static double stretchedRelease(const double ageSeconds,
                                   const double releaseSeconds) noexcept
    {
        const auto scaledAge = ageSeconds / std::max(1.0e-9, 1.779 * releaseSeconds);
        return std::exp(-std::pow(scaledAge, 1.286));
    }

    void beginRelease(const double detector) noexcept
    {
        releaseStartDb = gainReductionDb;
        releaseAgeSeconds = 0.0;
        releaseStartDetector = std::max(detectorEnvelope, detector);
        detectorEnvelope = releaseStartDetector;
        releasing = true;
    }

    double processAuto(const double targetDb, const double detectorLinear) noexcept
    {
        constexpr double peakReleaseSeconds = 0.08;
        constexpr double rmsSeconds = 0.05;
        const auto peakRelease = std::exp(-1.0 / (sampleRate * peakReleaseSeconds));
        const auto rmsCoefficient = std::exp(-1.0 / (sampleRate * rmsSeconds));
        peakEnvelope = detectorLinear > peakEnvelope ? detectorLinear
                                                      : peakRelease * peakEnvelope;
        rmsSquared = rmsCoefficient * rmsSquared
                   + (1.0 - rmsCoefficient) * detectorLinear * detectorLinear;
        const auto crest = peakEnvelope / std::max(std::sqrt(rmsSquared), 1.0e-9);
        const auto crestNorm = std::clamp((crest - 1.0) / 5.0, 0.0, 1.0);
        // Scientific crest-factor fallback until Auto black-box renders are available.
        const auto attackSeconds = 0.030 - 0.028 * crestNorm;
        const auto releaseSeconds = 0.600 - 0.480 * crestNorm;
        const auto coefficient = std::exp(-1.0 / (sampleRate
            * (targetDb > gainReductionDb ? attackSeconds : releaseSeconds)));
        gainReductionDb = coefficient * gainReductionDb + (1.0 - coefficient) * targetDb;
        return gainReductionDb;
    }

    double sampleRate { 48000.0 };
    double gainReductionDb {};
    double releaseStartDb {};
    double releaseAgeSeconds {};
    double detectorEnvelope {};
    double releaseStartDetector {};
    double rmsSquared {};
    double peakEnvelope {};
    bool releasing {};
    TCMode previousMode { TCMode::type1 };
};

class BiteProcessor final
{
public:
    void prepare(const double newSampleRate) noexcept
    {
        sampleRate = std::max(1.0, newSampleRate);
        reset();
    }

    void reset() noexcept
    {
        fastEnvelope = slowEnvelope = referencePeak = lastTransientNormalised = reliefMemoryDb = 0.0;
    }

    double getLastTransientNormalised() const noexcept { return lastTransientNormalised; }

    double process(const double normalGainReductionDb, const double detectorLinear,
                   const double biteValue) noexcept
    {
        const auto fastAttack = coefficient(0.0005);
        const auto fastRelease = coefficient(0.012);
        const auto slowAttack = coefficient(0.003);
        const auto slowRelease = coefficient(0.080);
        fastEnvelope = follow(fastEnvelope, detectorLinear, fastAttack, fastRelease);
        slowEnvelope = follow(slowEnvelope, detectorLinear, slowAttack, slowRelease);
        const auto referenceRelease = coefficient(0.150);
        referencePeak = detectorLinear > referencePeak ? detectorLinear
                                                       : referenceRelease * referencePeak;
        const auto transient = std::max(0.0, fastEnvelope - slowEnvelope);
        const auto transientNorm = std::clamp(
            transient / std::max(0.487 * referencePeak, 1.0e-12), 0.0, 1.0);
        lastTransientNormalised = transientNorm;
        const auto biteNorm = std::clamp((biteValue - 1.0) / 49.0, 0.0, 1.0);
        const auto requestedRelief = std::min(normalGainReductionDb,
                                              3.2 * transientShape(transientNorm));
        const auto memoryRelease = coefficient(0.018);
        reliefMemoryDb = requestedRelief > reliefMemoryDb ? requestedRelief
            : memoryRelease * reliefMemoryDb;
        const auto reliefDb = std::min(normalGainReductionDb, biteNorm * reliefMemoryDb);
        return std::max(0.0, normalGainReductionDb - reliefDb);
    }

private:
    double coefficient(const double seconds) const noexcept
    {
        return std::exp(-1.0 / (sampleRate * seconds));
    }

    static double follow(const double current, const double input,
                         const double attack, const double release) noexcept
    {
        const auto c = input > current ? attack : release;
        return c * current + (1.0 - c) * input;
    }

    static double transientShape(const double value) noexcept
    {
        const auto interpolate = [](const double x, const double x0, const double x1,
                                    const double y0, const double y1)
        {
            const auto t = std::clamp((x - x0) / (x1 - x0), 0.0, 1.0);
            const auto smooth = t * t * (3.0 - 2.0 * t);
            return y0 + (y1 - y0) * smooth;
        };
        // T13 full-chain fit: descriptors at maximum applicable GR are
        // 0.824 (100 ms), 0.867 (10 ms), and 0.973 (step).
        if (value <= 0.824) return 0.114 * value / 0.824;
        if (value <= 0.867) return interpolate(value, 0.824, 0.867, 0.114, 0.396);
        if (value <= 0.973) return interpolate(value, 0.867, 0.973, 0.396, 1.0);
        return 1.0;
    }

    double sampleRate { 48000.0 };
    double fastEnvelope {}, slowEnvelope {}, referencePeak {}, lastTransientNormalised {};
    double reliefMemoryDb {};
};

} // namespace pontedsp::mc2000::dsp

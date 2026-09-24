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
        cachedAttackMs = -1.0;
        cachedR1ReleaseSeconds = -1.0;
        autoRelease = std::exp(-1.0 / (sampleRate * 0.102));
        autoAttack = std::exp(-1.0 / (sampleRate * 0.00032));
        reset();
    }

    void reset() noexcept
    {
        gainReductionDb = releaseStartDb = releaseAgeSeconds = 0.0;
        detectorEnvelope = releaseStartDetector = 0.0;
        releasing = false;
        previousMode = TCMode::type1;
        autoControl = 1.0;
        previousAutoSlope = 0.0;
    }

    bool isQuiet() const noexcept
    {
        return gainReductionDb < 1.0e-9
            && (previousMode == TCMode::automatic || detectorEnvelope < 1.0e-24);
    }

    double process(const double targetDb, const double detectorLinear,
                   const double attackMs, const double releaseMs, const TCMode mode,
                   const double ratio = 2.0) noexcept
    {
        const auto target = clampFinite(targetDb, 0.0, 160.0, 0.0);
        const auto detector = clampFinite(detectorLinear, 0.0, 1.0e6, 0.0);

        if (mode == TCMode::automatic)
        {
            const auto slope = 1.0 - 1.0 / clampFinite(ratio, 1.0, 10.0, 2.0);
            if (slope <= 1.0e-9)
            {
                autoControl = 1.0;
                gainReductionDb = 0.0;
                previousAutoSlope = 0.0;
                previousMode = mode;
                return 0.0;
            }
            if (previousMode != mode || previousAutoSlope != slope)
                autoControl = decibelsToGain(std::min(160.0, gainReductionDb / slope));
            previousMode = mode;
            previousAutoSlope = slope;
            releasing = false;
            return processAuto(target, slope);
        }

        const auto displayedAttackMs = clampFinite(attackMs, 0.25, 250.0, 10.0);
        if (displayedAttackMs != cachedAttackMs)
        {
            // Same T08 calculation, only when its inputs change.
            const auto attackScale = 0.51 / (1.0 + displayedAttackMs / 800.0);
            const auto attackSeconds = displayedAttackMs * attackScale * 0.001;
            attackCoefficient = std::exp(-1.0 / (sampleRate * attackSeconds));
            cachedAttackMs = displayedAttackMs;
        }
        const auto releaseSeconds = clampFinite(releaseMs, 25.0, 2500.0, 250.0) * 0.001;

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

        if (mode == TCMode::type1)
        {
            // Isolated original probes (2026-09-20): excess linear control
            // decays with tau = displayed Release. Ratio scaling beyond 2:1
            // is a model extrapolation, not an original measurement.
            const auto slope = 1.0 - 1.0 / clampFinite(ratio, 1.0, 10.0, 2.0);
            if (releaseSeconds != cachedR1ReleaseSeconds)
            {
                r1ReleaseDelta = std::expm1(-1.0 / (sampleRate * releaseSeconds));
                cachedR1ReleaseSeconds = releaseSeconds;
            }
            const auto before = gainReductionDb;
            if (slope > 0.0)
            {
                constexpr double dbPerNeper = 8.685889638065036553;
                const auto scale = dbPerNeper * slope;
                // Equivalent to scale*log(1+(exp(g/scale)-1)*exp(-dt/tau)).
                // Incremental log form avoids overflow near ratio 1 and keeps
                // current GR continuous when ratio or Release is automated.
                gainReductionDb += scale * std::log1p(
                    -std::expm1(-gainReductionDb / scale) * r1ReleaseDelta);
            }
            else
                gainReductionDb = 0.0;
            if (gainReductionDb < 1.0e-8)
                gainReductionDb = 0.0;
            detectorEnvelope *= before > 0.0 ? gainReductionDb / before : 0.0;
            return gainReductionDb;
        }

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

    double processAuto(const double targetDb, const double slope) noexcept
    {
        // 2026-09-19 MC404 step fit: excess linear control decays with ~102 ms,
        // giving level-dependent release in dB. Ratio normalisation preserves
        // the measured identical release shape at 2:1 and 4:1. The 320 us
        // peak capture is a joint noise/onset fit (2026-09-23), not a
        // front-panel attack value. A noise-only 640 us fit worsens BITE onsets.
        // See Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-19.
        const auto targetControl = decibelsToGain(std::min(160.0, targetDb / slope));
        autoControl = targetControl > autoControl
            ? autoAttack * autoControl + (1.0 - autoAttack) * targetControl
            : 1.0 + autoRelease * (autoControl - 1.0);
        gainReductionDb = slope * gainToDecibels(autoControl);
        return gainReductionDb;
    }

    double sampleRate { 48000.0 };
    double cachedAttackMs { -1.0 }, attackCoefficient {};
    double cachedR1ReleaseSeconds { -1.0 }, r1ReleaseDelta {};
    double gainReductionDb {};
    double releaseStartDb {};
    double releaseAgeSeconds {};
    double detectorEnvelope {};
    double releaseStartDetector {};
    bool releasing {};
    TCMode previousMode { TCMode::type1 };
    double autoControl { 1.0 }, previousAutoSlope {};
    double autoAttack {}, autoRelease {};
};

class BiteProcessor final
{
public:
    void prepare(const double newSampleRate) noexcept
    {
        sampleRate = std::max(1.0, newSampleRate);
        fastAttack = coefficient(0.0005);
        fastRelease = coefficient(0.012);
        slowAttack = coefficient(0.003);
        slowRelease = coefficient(0.080);
        referenceRelease = coefficient(0.150);
        memoryRelease = coefficient(0.018);
        cachedBite = -1.0;
        reset();
    }

    void reset() noexcept
    {
        fastEnvelope = slowEnvelope = referencePeak = lastTransientNormalised = reliefMemoryDb = 0.0;
        autoGainReductionDb = 0.0;
    }

    bool isQuiet() const noexcept
    {
        return fastEnvelope < 1.0e-24 && slowEnvelope < 1.0e-24
            && referencePeak < 1.0e-24 && reliefMemoryDb < 1.0e-9
            && autoGainReductionDb < 1.0e-9;
    }

    double getLastTransientNormalised() const noexcept { return lastTransientNormalised; }

    double process(const double normalGainReductionDb, const double detectorLinear,
                   const double biteValue, const TCMode mode = TCMode::type1) noexcept
    {
        fastEnvelope = follow(fastEnvelope, detectorLinear, fastAttack, fastRelease);
        slowEnvelope = follow(slowEnvelope, detectorLinear, slowAttack, slowRelease);
        referencePeak = detectorLinear > referencePeak ? detectorLinear
                                                       : referenceRelease * referencePeak;
        const auto transient = std::max(0.0, fastEnvelope - slowEnvelope);
        const auto transientNorm = std::clamp(
            transient / std::max(0.487 * referencePeak, 1.0e-12), 0.0, 1.0);
        lastTransientNormalised = transientNorm;
        if (biteValue != cachedBite)
        {
            cachedBite = biteValue;
            biteNorm = controlShape(biteValue);
            // Auto BITE fit from isolated original 315 Hz / 2 kHz onsets:
            // ~0.65 ms at 5, ~3 ms at 10. Intermediate settings are a smooth
            // interpolation, not additional original measurements.
            const auto amount = std::clamp((biteValue - 1.0) / 9.0, 0.0, 1.0);
            const auto seconds = 0.003 * std::pow(amount, 1.875);
            autoBiteAttack = seconds > 0.0 ? coefficient(seconds) : 0.0;
        }
        const auto requestedRelief = std::min(normalGainReductionDb,
                                              3.2 * transientShape(transientNorm));
        reliefMemoryDb = requestedRelief > reliefMemoryDb ? requestedRelief
            : memoryRelease * reliefMemoryDb;
        if (mode == TCMode::automatic)
        {
            // Smooth rising GR, following falling GR immediately. The underlying
            // Auto detector retains its 102 ms memory and settled gain. Keep the
            // manual BITE envelopes warm for a subsequent mode change.
            autoGainReductionDb = normalGainReductionDb > autoGainReductionDb
                ? autoBiteAttack * autoGainReductionDb + (1.0 - autoBiteAttack) * normalGainReductionDb
                : normalGainReductionDb;
            return autoGainReductionDb;
        }
        autoGainReductionDb = normalGainReductionDb;
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

    static double controlShape(const double biteValue) noexcept
    {
        const auto interpolate = [] (const double x, const double x0, const double x1,
                                     const double y0, const double y1)
        {
            const auto t = std::clamp((x - x0) / (x1 - x0), 0.0, 1.0);
            const auto smooth = t * t * (3.0 - 2.0 * t);
            return y0 + (y1 - y0) * smooth;
        };
        const auto normalised = std::clamp((biteValue - 1.0) / 9.0, 0.0, 1.0);
        constexpr double biteFive = 4.0 / 9.0;
        // Preserve the calibrated gentle response around BITE=5 while mapping
        // the public 1..10 control range onto the full transient-relief range.
        if (normalised <= biteFive)
            return interpolate(normalised, 0.0, biteFive, 0.0, 0.062);
        return interpolate(normalised, biteFive, 1.0, 0.062, 1.0);
    }

    double sampleRate { 48000.0 };
    double fastAttack { coefficient(0.0005) }, fastRelease { coefficient(0.012) };
    double slowAttack { coefficient(0.003) }, slowRelease { coefficient(0.080) };
    double referenceRelease { coefficient(0.150) }, memoryRelease { coefficient(0.018) };
    double cachedBite { -1.0 }, biteNorm {};
    double fastEnvelope {}, slowEnvelope {}, referencePeak {}, lastTransientNormalised {};
    double reliefMemoryDb {};
    double autoGainReductionDb {}, autoBiteAttack {};
};

} // namespace pontedsp::mc2000::dsp

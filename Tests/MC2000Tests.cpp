#include "DSP/CrossoverNetwork.h"
#include "DSP/Ballistics.h"
#include "DSP/GainComputer.h"
#include "DSP/LinkwitzRiley4.h"
#include "DSP/MultiBandCompressor.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(const bool condition, const std::string& message)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectNear(const double actual, const double expected, const double tolerance,
                const std::string& message)
{
    expect(std::abs(actual - expected) <= tolerance,
           message + " (actual=" + std::to_string(actual)
           + ", expected=" + std::to_string(expected) + ")");
}

void testLinkwitzRiley()
{
    pontedsp::mc2000::dsp::LinkwitzRiley4 filter;
    filter.prepare(48000.0, 1000.0);
    double lowEnergy = 0.0, highEnergy = 0.0;
    constexpr int samples = 48000;
    for (int n = 0; n < samples; ++n)
    {
        const auto input = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * n / 48000.0);
        const auto [low, high] = filter.split(input);
        if (n > 12000)
        {
            lowEnergy += low * low;
            highEnergy += high * high;
        }
    }
    const auto count = static_cast<double>(samples - 12001);
    const auto lowRms = std::sqrt(lowEnergy / count);
    const auto highRms = std::sqrt(highEnergy / count);
    const auto inputRms = std::sqrt(0.5);
    expectNear(20.0 * std::log10(lowRms / inputRms), -6.0205999, 0.001, "LR4 low is -6.0206 dB at fc");
    expectNear(20.0 * std::log10(highRms / inputRms), -6.0205999, 0.001, "LR4 high is -6.0206 dB at fc");
}

void testFourBandFlatSum()
{
    pontedsp::mc2000::dsp::CrossoverNetwork network;
    network.prepare(48000.0, 1);
    network.setBandCount(4);
    network.setFrequencies({ 100.0, 1000.0, 10000.0 });
    double inputEnergy = 0.0, outputEnergy = 0.0;
    for (int n = 0; n < 96000; ++n)
    {
        const auto input = 0.7 * std::sin(2.0 * 3.14159265358979323846 * 2731.0 * n / 48000.0);
        std::array<double, 4> bands {};
        network.processSample(0, input, bands);
        const auto output = bands[0] + bands[1] + bands[2] + bands[3];
        if (n > 24000)
        {
            inputEnergy += input * input;
            outputEnergy += output * output;
        }
    }
    expectNear(10.0 * std::log10(outputEnergy / inputEnergy), 0.0, 0.002,
               "compensated four-band sum has unity magnitude");
}

void testGainComputer()
{
    pontedsp::mc2000::dsp::GainComputer gain;
    expectNear(gain.computeOutputDb(-30.0, -24.0, 4.0, 0.0), -30.0, 1.0e-12,
               "hard knee leaves input below threshold unchanged");
    expectNear(gain.computeOutputDb(-12.0, -24.0, 4.0, 0.0), -21.0, 1.0e-12,
               "hard knee uses exact ratio");
    expectNear(-24.0 - gain.computeOutputDb(-24.0, -24.0, 4.0, -10.0), 4.357, 0.01,
               "negative knee anchor matches measured maximum");
    expect(gain.computeOutputDb(-18.0, -24.0, 4.0, 5.0)
           > gain.computeOutputDb(-18.0, -24.0, 4.0, 0.0),
           "positive knee reduces gain reduction around onset");

    struct Anchor { double input, knee, delta; };
    const std::array anchors {
        Anchor {-36.0, -10.0, -1.381731}, Anchor {-28.0, -5.0, -0.738409},
        Anchor {-24.0, -5.0, -2.514705}, Anchor {-24.0, -10.0, -4.356975},
        Anchor {-18.0, 5.0, 1.363561}, Anchor {-17.0, 10.0, 1.923875},
        Anchor {-3.0, 12.5, 2.061559}, Anchor {-3.0, 15.0, 4.068932}
    };
    for (const auto& anchor : anchors)
    {
        const auto hard = gain.computeOutputDb(anchor.input, -24.0, 4.0, 0.0);
        const auto measured = gain.computeOutputDb(anchor.input, -24.0, 4.0, anchor.knee) - hard;
        const auto tolerance = anchor.knee == 15.0 ? 0.06 : 1.0e-5;
        expectNear(measured, anchor.delta, tolerance, "measured Knee LUT anchor");
    }

    expectNear(gain.computeOutputDb(-30.0, -24.0, 4.0, -5.0), -30.0, 1.0e-12,
               "Knee -5 has no correction six dB below threshold");
}

void testBallisticsModels()
{
    using namespace pontedsp::mc2000::dsp;
    constexpr double sampleRate = 48000.0;
    Ballistics type1;
    type1.prepare(sampleRate);
    auto gr = 0.0;
    for (int n = 0; n < static_cast<int>(sampleRate * 0.5); ++n)
        gr = type1.process(12.0, 1.0, 0.03, 100.0, TCMode::type1);
    const auto releaseStart = gr;
    for (int n = 0; n < static_cast<int>(sampleRate * 0.1336); ++n)
        gr = type1.process(0.0, 0.0, 0.03, 100.0, TCMode::type1);
    expectNear(gr / releaseStart, 0.5, 0.003, "Type-1 measured release reaches half at 1.336 R");

    Ballistics seed;
    seed.prepare(sampleRate);
    for (int n = 0; n < static_cast<int>(sampleRate * 0.2); ++n)
        seed.process(12.0, 1.0, 0.03, 1000.0, TCMode::type1);
    auto quiet = seed;
    auto lowerEvent = seed;
    for (int n = 0; n < 4800; ++n)
    {
        const auto a = quiet.process(0.0, 0.0, 0.03, 1000.0, TCMode::type1);
        const auto b = lowerEvent.process(2.0, 0.2, 0.03, 1000.0, TCMode::type1);
        expectNear(a, b, 1.0e-12, "Type-1 ignores a second event below its release envelope");
    }

    Ballistics type2Quiet;
    Ballistics type2Event;
    type2Quiet.prepare(sampleRate);
    type2Event.prepare(sampleRate);
    for (int n = 0; n < static_cast<int>(sampleRate * 0.2); ++n)
    {
        type2Quiet.process(12.0, 1.0, 0.03, 1000.0, TCMode::type2);
        type2Event.process(12.0, 1.0, 0.03, 1000.0, TCMode::type2);
    }
    auto quietGr = 0.0;
    auto eventGr = 0.0;
    for (int n = 0; n < 4800; ++n)
    {
        quietGr = type2Quiet.process(0.0, 0.0, 0.03, 1000.0, TCMode::type2);
        eventGr = type2Event.process(2.0, 0.25, 0.03, 1000.0, TCMode::type2);
    }
    expect(eventGr > quietGr, "Type-2 lower event slows the measured adaptive release");

    Ballistics autoFastManual;
    Ballistics autoSlowManual;
    autoFastManual.prepare(sampleRate);
    autoSlowManual.prepare(sampleRate);
    for (int n = 0; n < 24000; ++n)
    {
        const auto detector = n < 12000 ? 0.05 : 0.8;
        const auto target = n < 12000 ? 0.0 : 10.0;
        const auto fast = autoFastManual.process(target, detector, 0.03, 5.0, TCMode::automatic);
        const auto slow = autoSlowManual.process(target, detector, 250.0, 2500.0, TCMode::automatic);
        expectNear(fast, slow, 1.0e-12,
                   "Auto computes timing from the signal and ignores manual Attack/Release");
    }

    for (const auto rate : {44100.0, 96000.0})
    {
        Ballistics scaled;
        scaled.prepare(rate);
        auto current = 0.0;
        for (int n = 0; n < static_cast<int>(rate * 0.5); ++n)
            current = scaled.process(12.0, 1.0, 0.03, 100.0, TCMode::type1);
        const auto start = current;
        for (int n = 0; n < static_cast<int>(rate * 0.1336); ++n)
            current = scaled.process(0.0, 0.0, 0.03, 100.0, TCMode::type1);
        expectNear(current / start, 0.5, 0.003,
                   "Type-1 timing remains invariant across sample rates");
    }
}

double maximumBiteRelief(const double riseSeconds)
{
    using namespace pontedsp::mc2000::dsp;
    constexpr double sampleRate = 48000.0;
    BiteProcessor bite;
    bite.prepare(sampleRate);
    const auto riseSamples = static_cast<int>(riseSeconds * sampleRate);
    auto maximum = 0.0;
    for (int n = 0; n < static_cast<int>(0.4 * sampleRate); ++n)
    {
        const auto detector = riseSamples == 0 ? 1.0
            : std::min(1.0, static_cast<double>(n + 1) / riseSamples);
        maximum = std::max(maximum, 6.0 - bite.process(6.0, detector, 50.0));
    }
    return maximum;
}

void testBiteModel()
{
    using namespace pontedsp::mc2000::dsp;
    BiteProcessor bite;
    bite.prepare(48000.0);
    expectNear(bite.process(6.0, 1.0, 1.0), 6.0, 1.0e-12,
               "minimum BITE is exactly neutral");
    const auto step = maximumBiteRelief(0.0);
    expect(step <= 3.2 + 1.0e-12, "BITE relief respects calibrated onset ceiling");
    for (int n = 0; n < 48000; ++n)
        bite.process(6.0, 1.0, 50.0);
    expectNear(bite.process(6.0, 1.0, 50.0), 6.0, 0.001,
               "BITE converges to neutral gain at steady state");
}

void testStereoDetectorAndFiniteOutput()
{
    using namespace pontedsp::mc2000::dsp;
    MultiBandCompressor compressor;
    GlobalParameters parameters;
    parameters.numBands = 2;
    for (auto& band : parameters.bands)
    {
        band.thresholdDb = -24.0;
        band.ratio = 4.0;
        band.attackMs = 0.03;
        band.releaseMs = 250.0;
    }
    compressor.setParameters(parameters);
    compressor.prepare(48000.0, 512, 2);
    std::vector<float> left(512), right(512);
    std::array<float*, 2> pointers { left.data(), right.data() };
    for (int block = 0; block < 100; ++block)
    {
        for (int n = 0; n < 512; ++n)
        {
            const auto sample = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846
                * 1000.0 * (block * 512 + n) / 48000.0));
            left[static_cast<std::size_t>(n)] = sample;
            right[static_cast<std::size_t>(n)] = -sample;
        }
        compressor.process(pointers.data(), 2, 512);
        for (int n = 0; n < 512; ++n)
        {
            expect(std::isfinite(left[static_cast<std::size_t>(n)])
                   && std::isfinite(right[static_cast<std::size_t>(n)]), "output remains finite");
            expectNear(left[static_cast<std::size_t>(n)], -right[static_cast<std::size_t>(n)], 1.0e-6,
                       "antiphase channels receive identical gain reduction");
        }
    }
    expect(compressor.getBandMeter(0).gainReductionDb > 0.0f
           || compressor.getBandMeter(1).gainReductionDb > 0.0f,
           "stereo max detector triggers compression without antiphase cancellation");
}

void testArbitraryBlocksAndInvalidInput()
{
    using namespace pontedsp::mc2000::dsp;
    MultiBandCompressor compressor;
    GlobalParameters parameters;
    parameters.numBands = 4;
    compressor.setParameters(parameters);
    compressor.prepare(48000.0, 16, 2);
    std::vector<float> left(257, 0.1f), right(257, -0.1f);
    left[10] = std::numeric_limits<float>::quiet_NaN();
    right[20] = std::numeric_limits<float>::infinity();
    std::array<float*, 2> pointers {left.data(), right.data()};
    compressor.process(pointers.data(), 2, 257);
    for (int n = 0; n < 257; ++n)
        expect(std::isfinite(left[static_cast<std::size_t>(n)])
               && std::isfinite(right[static_cast<std::size_t>(n)]),
               "arbitrary block sizes and invalid samples cannot poison DSP state");
}

} // namespace

int main()
{
    testLinkwitzRiley();
    testFourBandFlatSum();
    testGainComputer();
    testBallisticsModels();
    testBiteModel();
    testStereoDetectorAndFiniteOutput();
    testArbitraryBlocksAndInvalidInput();
    if (failures == 0)
        std::cout << "All Ponte MC2000 DSP tests passed\n";
    return failures == 0 ? 0 : 1;
}

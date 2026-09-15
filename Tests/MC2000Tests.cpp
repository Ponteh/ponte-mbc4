#include "DSP/CrossoverNetwork.h"
#include "DSP/Ballistics.h"
#include "DSP/GainComputer.h"
#include "DSP/LinkwitzRiley4.h"
#include "DSP/MultiBandCompressor.h"
#include "UI/MeterBallistics.h"
#include <thread>
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
        gr = type1.process(12.0, 1.0, 0.25, 100.0, TCMode::type1);
    const auto releaseStart = gr;
    for (int n = 0; n < static_cast<int>(sampleRate * 0.1336); ++n)
        gr = type1.process(0.0, 0.0, 0.25, 100.0, TCMode::type1);
    expectNear(gr / releaseStart, 0.5, 0.003, "Type-1 measured release reaches half at 1.336 R");

    Ballistics seed;
    seed.prepare(sampleRate);
    for (int n = 0; n < static_cast<int>(sampleRate * 0.2); ++n)
        seed.process(12.0, 1.0, 0.25, 1000.0, TCMode::type1);
    auto quiet = seed;
    auto lowerEvent = seed;
    for (int n = 0; n < 4800; ++n)
    {
        const auto a = quiet.process(0.0, 0.0, 0.25, 1000.0, TCMode::type1);
        const auto b = lowerEvent.process(2.0, 0.2, 0.25, 1000.0, TCMode::type1);
        expectNear(a, b, 1.0e-12, "Type-1 ignores a second event below its release envelope");
    }

    Ballistics type2Quiet;
    Ballistics type2Event;
    type2Quiet.prepare(sampleRate);
    type2Event.prepare(sampleRate);
    for (int n = 0; n < static_cast<int>(sampleRate * 0.2); ++n)
    {
        type2Quiet.process(12.0, 1.0, 0.25, 1000.0, TCMode::type2);
        type2Event.process(12.0, 1.0, 0.25, 1000.0, TCMode::type2);
    }
    auto quietGr = 0.0;
    auto eventGr = 0.0;
    for (int n = 0; n < 4800; ++n)
    {
        quietGr = type2Quiet.process(0.0, 0.0, 0.25, 1000.0, TCMode::type2);
        eventGr = type2Event.process(2.0, 0.25, 0.25, 1000.0, TCMode::type2);
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
        const auto fast = autoFastManual.process(target, detector, 0.25, 25.0, TCMode::automatic);
        const auto slow = autoSlowManual.process(target, detector, 250.0, 2500.0, TCMode::automatic);
        expectNear(fast, slow, 1.0e-12,
                   "Auto computes timing from the signal and ignores manual Attack/Release");
    }

    for (const auto rate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0})
    {
        Ballistics scaled;
        scaled.prepare(rate);
        auto current = 0.0;
        for (int n = 0; n < static_cast<int>(rate * 0.5); ++n)
            current = scaled.process(12.0, 1.0, 0.25, 100.0, TCMode::type1);
        const auto start = current;
        for (int n = 0; n < static_cast<int>(rate * 0.1336); ++n)
            current = scaled.process(0.0, 0.0, 0.25, 100.0, TCMode::type1);
        expectNear(current / start, 0.5, 0.003,
                   "Type-1 timing remains invariant across sample rates");
    }
}

double maximumBiteRelief(const double riseSeconds, const double biteValue = 10.0)
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
        maximum = std::max(maximum, 6.0 - bite.process(6.0, detector, biteValue));
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
    const auto biteFive = maximumBiteRelief(0.0, 5.0);
    const auto biteTen = maximumBiteRelief(0.0, 10.0);
    expect(biteTen > biteFive * 4.0,
           "BITE 10 follows the validated nonlinear low-range control response");
    for (int n = 0; n < 48000; ++n)
        bite.process(6.0, 1.0, 10.0);
    expectNear(bite.process(6.0, 1.0, 10.0), 6.0, 0.001,
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
        band.attackMs = 0.25;
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

void testExternalSidechain()
{
    using namespace pontedsp::mc2000::dsp;
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 512;
    GlobalParameters parameters;
    parameters.numBands = 2;
    parameters.crossoverHz = { 100.0, 1000.0, 10000.0 };
    for (auto& band : parameters.bands)
    {
        band.thresholdDb = -24.0;
        band.ratio = 10.0;
        band.attackMs = 0.25;
        band.releaseMs = 250.0;
        band.bite = 1.0;
    }

    MultiBandCompressor internalDetector;
    MultiBandCompressor externalDetector;
    internalDetector.setParameters(parameters);
    externalDetector.setParameters(parameters);
    internalDetector.prepare(sampleRate, blockSize, 2);
    externalDetector.prepare(sampleRate, blockSize, 2);

    std::vector<float> quietLeft(blockSize), quietRight(blockSize);
    std::vector<float> keyedLeft(blockSize), keyedRight(blockSize);
    std::vector<float> keyLeft(blockSize), keyRight(blockSize);
    std::array<float*, 2> quiet { quietLeft.data(), quietRight.data() };
    std::array<float*, 2> keyed { keyedLeft.data(), keyedRight.data() };
    std::array<const float*, 2> key { keyLeft.data(), keyRight.data() };
    auto quietEnergy = 0.0;
    auto keyedEnergy = 0.0;

    for (int block = 0; block < 300; ++block)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto phase = 2.0 * 3.14159265358979323846 * 100.0
                             * (block * blockSize + sample) / sampleRate;
            const auto program = static_cast<float>(0.02 * std::sin(phase));
            const auto sidechain = static_cast<float>(0.8 * std::sin(phase));
            quietLeft[static_cast<std::size_t>(sample)] = program;
            quietRight[static_cast<std::size_t>(sample)] = -program;
            keyedLeft[static_cast<std::size_t>(sample)] = program;
            keyedRight[static_cast<std::size_t>(sample)] = -program;
            keyLeft[static_cast<std::size_t>(sample)] = sidechain;
            keyRight[static_cast<std::size_t>(sample)] = -sidechain;
        }
        internalDetector.process(quiet.data(), 2, blockSize);
        externalDetector.process(keyed.data(), 2, key.data(), 2, blockSize);
        if (block >= 200)
            for (int sample = 0; sample < blockSize; ++sample)
            {
                quietEnergy += quietLeft[static_cast<std::size_t>(sample)]
                             * quietLeft[static_cast<std::size_t>(sample)];
                keyedEnergy += keyedLeft[static_cast<std::size_t>(sample)]
                             * keyedLeft[static_cast<std::size_t>(sample)];
            }
    }

    expect(keyedEnergy < quietEnergy * 0.2,
           "external sidechain replaces program detection and compresses the program");
    expect(externalDetector.getBandMeter(0).gainReductionDb > 1.0f
           || externalDetector.getBandMeter(1).gainReductionDb > 1.0f,
           "external sidechain publishes per-band gain reduction");
}

void testExternalSidechainAcrossTimeConstants()
{
    using namespace pontedsp::mc2000::dsp;
    constexpr auto sampleRate = 48000.0;
    constexpr auto blockSize = 256;
    for (const auto mode : { TCMode::type2, TCMode::automatic })
    {
        GlobalParameters parameters;
        parameters.numBands = 2;
        parameters.crossoverHz = { 300.0, 3000.0, 10000.0 };
        for (auto& band : parameters.bands)
        {
            band.thresholdDb = -24.0;
            band.ratio = 10.0;
            band.attackMs = 0.25;
            band.releaseMs = 250.0;
            band.bite = 1.0;
            band.tcMode = mode;
        }

        MultiBandCompressor internalDetector;
        MultiBandCompressor externalDetector;
        internalDetector.setParameters(parameters);
        externalDetector.setParameters(parameters);
        internalDetector.prepare(sampleRate, blockSize, 2);
        externalDetector.prepare(sampleRate, blockSize, 2);

        std::vector<float> internalLeft(blockSize), internalRight(blockSize);
        std::vector<float> externalLeft(blockSize), externalRight(blockSize);
        std::vector<float> keyLeft(blockSize), keyRight(blockSize);
        std::array<float*, 2> internal { internalLeft.data(), internalRight.data() };
        std::array<float*, 2> external { externalLeft.data(), externalRight.data() };
        std::array<const float*, 2> key { keyLeft.data(), keyRight.data() };
        auto internalEnergy = 0.0;
        auto externalEnergy = 0.0;

        for (int block = 0; block < 400; ++block)
        {
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const auto phase = 2.0 * 3.14159265358979323846 * 1000.0
                                 * (block * blockSize + sample) / sampleRate;
                const auto program = static_cast<float>(0.005 * std::sin(phase));
                const auto sidechain = static_cast<float>(0.8 * std::sin(phase));
                internalLeft[static_cast<std::size_t>(sample)] = program;
                internalRight[static_cast<std::size_t>(sample)] = -program;
                externalLeft[static_cast<std::size_t>(sample)] = program;
                externalRight[static_cast<std::size_t>(sample)] = -program;
                keyLeft[static_cast<std::size_t>(sample)] = sidechain;
                keyRight[static_cast<std::size_t>(sample)] = -sidechain;
            }
            internalDetector.process(internal.data(), 2, blockSize);
            externalDetector.process(external.data(), 2, key.data(), 2, blockSize);
            if (block >= 300)
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    internalEnergy += internalLeft[static_cast<std::size_t>(sample)]
                                    * internalLeft[static_cast<std::size_t>(sample)];
                    externalEnergy += externalLeft[static_cast<std::size_t>(sample)]
                                    * externalLeft[static_cast<std::size_t>(sample)];
                }
        }

        const auto modeName = mode == TCMode::type2 ? "Type-2" : "Auto";
        expect(externalEnergy < internalEnergy * 0.6,
               std::string("external sidechain controls ") + modeName + " gain reduction");
    }
}

void testSoloSmoothing()
{
    using namespace pontedsp::mc2000::dsp;
    constexpr auto blockSize = 512;
    MultiBandCompressor compressor;
    GlobalParameters parameters;
    parameters.numBands = 2;
    parameters.crossoverHz = { 1000.0, 5000.0, 10000.0 };
    compressor.setParameters(parameters);
    compressor.prepare(48000.0, blockSize, 2);

    std::vector<float> left(blockSize, 0.1f), right(blockSize, 0.1f);
    std::array<float*, 2> channels { left.data(), right.data() };
    for (int block = 0; block < 30; ++block)
    {
        std::fill(left.begin(), left.end(), 0.1f);
        std::fill(right.begin(), right.end(), 0.1f);
        compressor.process(channels.data(), 2, blockSize);
    }

    parameters.bands[1].solo = true;
    compressor.setParameters(parameters);
    left[0] = right[0] = 0.1f;
    compressor.process(channels.data(), 2, 1);
    expect(left[0] > 0.08f && right[0] > 0.08f,
           "Solo automation crossfades instead of muting a band on one sample");

    for (int block = 0; block < 4; ++block)
    {
        std::fill(left.begin(), left.end(), 0.1f);
        std::fill(right.begin(), right.end(), 0.1f);
        compressor.process(channels.data(), 2, blockSize);
    }
    expect(std::abs(left.back()) < 0.01f && std::abs(right.back()) < 0.01f,
           "Solo crossfade reaches the selected band after its smoothing interval");
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

void testMeterCapture()
{
    using namespace pontedsp::mc2000::dsp;
    // A short tone may end many blocks before the GUI samples its mailbox.
    for (const auto blockSize : { 64, 512, 1024 })
    {
        MultiBandCompressor engine, unobserved;
        GlobalParameters parameters;
        for (auto& band : parameters.bands)
        {
            band.thresholdDb = -27.5;
            band.ratio = 2.0;
            band.attackMs = 2.5;
        }
        engine.setParameters(parameters);
        unobserved.setParameters(parameters);
        engine.prepare(48000, blockSize, 2);
        unobserved.prepare(48000, blockSize, 2);
        std::array<BandMeterSnapshot, 4> expected;
        std::array<float, 2> master { -100.0f, -100.0f };
        for (int start = 0; start < 48000; start += blockSize)
        {
            const auto count = std::min(blockSize, 48000 - start);
            std::vector<float> left(count), right(count);
            for (int n = 0; n < count; ++n)
                if (start + n < 480)
                {
                    left[n] = static_cast<float>(.5 * std::sin(2 * 3.141592653589793 * 315 * (start + n) / 48000));
                    right[n] = left[n] * .25f;
                }
            auto otherLeft = left, otherRight = right;
            float* audio[] { left.data(), right.data() };
            float* otherAudio[] { otherLeft.data(), otherRight.data() };
            engine.process(audio, 2, count);
            unobserved.process(otherAudio, 2, count);
            expect(left == otherLeft && right == otherRight,
                   "GUI consumption has no effect on rendered audio");
            for (int band = 0; band < 4; ++band)
            {
                const auto raw = engine.getBandMeter(band);
                expected[band].inputDb = std::max(expected[band].inputDb, raw.inputDb);
                expected[band].outputDb = std::max(expected[band].outputDb, raw.outputDb);
                expected[band].gainReductionDb = std::max(expected[band].gainReductionDb, raw.gainReductionDb);
            }
            const auto rawMaster = engine.getOutputMeterDb();
            for (int ch = 0; ch < 2; ++ch) master[ch] = std::max(master[ch], rawMaster[ch]);
            // Exercise independent GUI polling on the comparison instance.
            if (start % 3 == 0) unobserved.discardPendingMeterPeaks();
        }
        for (int band = 0; band < 4; ++band)
        {
            const auto captured = engine.consumeBandMeter(band);
            expectNear(captured.inputDb, expected[band].inputDb, 0.0, "IN retains every block peak");
            expectNear(captured.outputDb, expected[band].outputDb, 0.0, "OUT retains every block peak");
            expectNear(captured.gainReductionDb, expected[band].gainReductionDb, 0.0, "GR retains every block peak");
            const auto empty = engine.consumeBandMeter(band);
            expect(empty.inputDb == -100 && empty.outputDb == -100 && empty.gainReductionDb == 0,
                   "consumed band mailbox returns silence until another audio block");
        }
        expect(engine.consumeOutputMeterDb() == master, "both MAIN peaks survive intervening silence");
        expectNear(master[0] - master[1], 12.0412, .001, "stereo MAIN channels stay independent");
        expect(engine.getOutputMeterDb()[0] < -90, "raw analysis getter still reflects the latest block");
        engine.reset();
        expect(engine.consumeOutputMeterDb()[0] == -100 && engine.getBandMeter(1).gainReductionDb == 0,
               "reset clears raw readings and pending peaks");
    }

    // Concurrent publish/exchange: a peak must be delivered either to the
    // racing consumer or to its next read, never lost between load and reset.
    MeterPeak peak;
    std::atomic<bool> done { false };
    std::thread producer([&]
    {
        for (int i = 0; i < 100000; ++i) peak.publish(static_cast<float>(i));
        done.store(true, std::memory_order_release);
    });
    float maximum = -100;
    while (!done.load(std::memory_order_acquire)) maximum = std::max(maximum, peak.consume());
    producer.join();
    maximum = std::max(maximum, peak.consume());
    expect(maximum == 99999, "concurrent GUI exchange cannot erase the final audio peak");
}

void testVisualMeterBallistics()
{
    using namespace pontedsp::gui;
    for (const auto drop : { 6.0, 12.0 })
    {
        LevelMeterBallistics meter;
        meter.update(-6, 0);
        double t10 = 0, t90 = 0, previous = -6;
        for (int ms = 1; ms <= 2000; ++ms)
        {
            const auto value = meter.update(-6 - drop, .001);
            expect(value <= previous + 1.e-10 && value >= -6 - drop - 1.e-10,
                   "level return is monotonic and does not overshoot");
            if (!t10 && value <= -6 - .1 * drop) t10 = ms * .001;
            if (!t90 && value <= -6 - .9 * drop) t90 = ms * .001;
            previous = value;
        }
        expectNear(t90 - t10, drop == 6 ? .467 : .717, .035,
                   "level fall reproduces measured original step interval");
    }
    LevelMeterBallistics slow, fast, irregular;
    for (auto* meter : { &slow, &fast, &irregular }) meter->update(-6, 0);
    for (int n = 0; n < 30; ++n) slow.update(-48, 1.0 / 30);
    for (int n = 0; n < 60; ++n) fast.update(-48, 1.0 / 60);
    for (auto dt : { .011, .173, .016, .3, .5 }) irregular.update(-48, dt);
    expectNear(slow.value(), fast.value(), 1.e-10, "level timing is independent of refresh rate");
    expectNear(slow.value(), irregular.value(), 1.e-10, "level timing handles GUI jitter");
    expectNear(slow.update(-2, .033), -2, 0, "fresh peaks attack immediately");
    expect(slow.update(-100, 12) < -99, "level meter drains when audio callbacks stop");
    GainReductionMeterBallistics gr;
    expectNear(gr.update(10.5, .033), 10.5, 0, "GR plateau has no artificial calibration offset");
    gr.update(0, 3);
    expect(gr.value() < .001, "GR drains without new audio callbacks");
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
    testExternalSidechain();
    testExternalSidechainAcrossTimeConstants();
    testSoloSmoothing();
    testArbitraryBlocksAndInvalidInput();
    testMeterCapture();
    testVisualMeterBallistics();
    if (failures == 0)
        std::cout << "All Ponte MC2000 DSP tests passed\n";
    return failures == 0 ? 0 : 1;
}

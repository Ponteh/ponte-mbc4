#include "MultiBandCompressor.h"
#include "Db.h"

namespace pontedsp::mc2000::dsp {

void MultiBandCompressor::prepare(const double newSampleRate, const int maxBlockSize,
                                  const int numChannels)
{
    sampleRate = std::max(1.0, newSampleRate);
    preparedBlockSize = std::max(1, maxBlockSize);
    preparedChannels = std::clamp(numChannels, 1, maxChannels);
    crossover.prepare(sampleRate, preparedChannels);
    for (auto& state : ballistics) state.prepare(sampleRate);
    for (auto& bite : biteProcessors) bite.prepare(sampleRate);
    inputGainCurrent = decibelsToGain(currentParameters.inputGainDb);
    outputGainCurrent = decibelsToGain(currentParameters.outputGainDb);
    for (int band = 0; band < maxBands; ++band)
    {
        const auto& p = currentParameters.bands[static_cast<std::size_t>(band)];
        bandGainCurrent[static_cast<std::size_t>(band)] = decibelsToGain(p.gainDb);
        enabledMixCurrent[static_cast<std::size_t>(band)] = p.enabled ? 1.0 : 0.0;
    }
    crossoverCurrent = currentParameters.crossoverHz;
    crossover.setBandCount(currentParameters.numBands);
    crossover.setFrequencies(crossoverCurrent);
    reset();
}

void MultiBandCompressor::reset() noexcept
{
    crossover.reset();
    for (auto& state : ballistics) state.reset();
    for (auto& bite : biteProcessors) bite.reset();
}

void MultiBandCompressor::setParameters(const GlobalParameters& parameters) noexcept
{
    currentParameters = parameters;
    currentParameters.numBands = std::clamp(parameters.numBands, 2, maxBands);
    currentParameters.inputGainDb = clampFinite(parameters.inputGainDb, -24.0, 24.0, 0.0);
    currentParameters.outputGainDb = clampFinite(parameters.outputGainDb, -24.0, 24.0, 0.0);
    auto& x = currentParameters.crossoverHz;
    x[0] = clampFinite(x[0], 20.0, 18000.0, 100.0);
    x[1] = clampFinite(x[1], x[0] + 1.0, 19000.0, 1000.0);
    x[2] = clampFinite(x[2], x[1] + 1.0, 20000.0, 10000.0);
    for (auto& band : currentParameters.bands)
    {
        band.gainDb = clampFinite(band.gainDb, 0.0, 48.0, 0.0);
        band.thresholdDb = clampFinite(band.thresholdDb, -45.0, 0.0, 0.0);
        band.ratio = clampFinite(band.ratio, 1.0, 10.0, 1.0);
        band.knee = clampFinite(band.knee, -10.0, 15.0, 0.0);
        band.bite = clampFinite(band.bite, 1.0, 50.0, 1.0);
        band.attackMs = clampFinite(band.attackMs, 0.03, 250.0, 10.0);
        band.releaseMs = clampFinite(band.releaseMs, 5.0, 2500.0, 250.0);
        const auto mode = std::clamp(static_cast<int>(band.tcMode), 0, 2);
        band.tcMode = static_cast<TCMode>(mode);
    }
    crossover.setBandCount(currentParameters.numBands);
}

double MultiBandCompressor::smoothGain(const double current, const double target,
                                       const double coefficient) noexcept
{
    return coefficient * current + (1.0 - coefficient) * target;
}

void MultiBandCompressor::process(float** channels, const int channelCount,
                                  const int sampleCount) noexcept
{
    if (channels == nullptr || channelCount <= 0 || sampleCount <= 0)
        return;
    const auto channelsToProcess = std::clamp(channelCount, 1, preparedChannels);
    for (int channel = 0; channel < channelsToProcess; ++channel)
        if (channels[channel] == nullptr) return;
    const auto bandsToProcess = currentParameters.numBands;
    const auto inputTarget = decibelsToGain(currentParameters.inputGainDb);
    const auto outputTarget = decibelsToGain(currentParameters.outputGainDb);
    const auto smoothing = std::exp(-1.0 / (sampleRate * 0.02));
    const auto bypassSmoothing = std::exp(-1.0 / (sampleRate * 0.005));
    const auto crossoverSmoothing = 1.0 - std::exp(-static_cast<double>(sampleCount)
                                                   / (sampleRate * 0.02));
    for (int index = 0; index < 3; ++index)
        crossoverCurrent[static_cast<std::size_t>(index)] += crossoverSmoothing
            * (currentParameters.crossoverHz[static_cast<std::size_t>(index)]
               - crossoverCurrent[static_cast<std::size_t>(index)]);
    crossoverCurrent[1] = std::max(crossoverCurrent[1], crossoverCurrent[0] + 1.0);
    crossoverCurrent[2] = std::max(crossoverCurrent[2], crossoverCurrent[1] + 1.0);
    crossover.setFrequencies(crossoverCurrent);
    const auto anySolo = [&]
    {
        for (int band = 0; band < bandsToProcess; ++band)
            if (currentParameters.bands[static_cast<std::size_t>(band)].solo) return true;
        return false;
    }();

    std::array<double, maxBands> inputPeaks {};
    std::array<double, maxBands> outputPeaks {};
    std::array<double, maxBands> maximumGr {};
    std::array<double, 2> masterPeaks {};

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        inputGainCurrent = smoothGain(inputGainCurrent, inputTarget, smoothing);
        outputGainCurrent = smoothGain(outputGainCurrent, outputTarget, smoothing);

        std::array<std::array<double, maxChannels>, maxBands> bandSamples {};
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            std::array<double, maxBands> splitBands {};
            const auto rawInput = static_cast<double>(channels[channel][sample]);
            const auto finiteInput = std::isfinite(rawInput) ? rawInput : 0.0;
            crossover.processSample(channel, finiteInput * inputGainCurrent,
                                    splitBands);
            for (int band = 0; band < bandsToProcess; ++band)
                bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] =
                    splitBands[static_cast<std::size_t>(band)];
        }

        for (int band = 0; band < bandsToProcess; ++band)
        {
            auto detector = 0.0;
            for (int channel = 0; channel < channelsToProcess; ++channel)
                detector = std::max(detector, std::abs(static_cast<double>(
                    bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)])));
            inputPeaks[static_cast<std::size_t>(band)] = std::max(inputPeaks[static_cast<std::size_t>(band)], detector);

            const auto& p = currentParameters.bands[static_cast<std::size_t>(band)];
            auto targetGr = 0.0;
            if (detector > 1.0e-12)
                targetGr = gainComputer.computeGainReductionDb(gainToDecibels(detector),
                                                                p.thresholdDb, p.ratio, p.knee);
            auto gr = ballistics[static_cast<std::size_t>(band)].process(
                targetGr, detector, p.attackMs, p.releaseMs, p.tcMode);
            gr = biteProcessors[static_cast<std::size_t>(band)].process(gr, detector, p.bite);
            auto& enabledMix = enabledMixCurrent[static_cast<std::size_t>(band)];
            enabledMix = smoothGain(enabledMix, p.enabled ? 1.0 : 0.0, bypassSmoothing);
            const auto appliedGr = gr * enabledMix;
            maximumGr[static_cast<std::size_t>(band)] = std::max(
                maximumGr[static_cast<std::size_t>(band)], appliedGr);
            auto& bandGain = bandGainCurrent[static_cast<std::size_t>(band)];
            bandGain = smoothGain(bandGain, decibelsToGain(p.gainDb), smoothing);
            const auto appliedBandGain = bandGain * decibelsToGain(-appliedGr);
            const auto audible = !anySolo || p.solo;

            for (int channel = 0; channel < channelsToProcess; ++channel)
            {
                auto& value = bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)];
                value = audible ? value * appliedBandGain : 0.0;
                outputPeaks[static_cast<std::size_t>(band)] = std::max(
                    outputPeaks[static_cast<std::size_t>(band)], std::abs(value));
            }
        }

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto output = 0.0;
            for (int band = 0; band < bandsToProcess; ++band)
                output += bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)];
            output *= outputGainCurrent * (currentParameters.phaseInvert ? -1.0 : 1.0);
            if (!std::isfinite(output)) output = 0.0;
            channels[channel][sample] = static_cast<float>(output);
            masterPeaks[static_cast<std::size_t>(channel)] = std::max(
                masterPeaks[static_cast<std::size_t>(channel)], std::abs(output));
        }
    }

    publishMeters(inputPeaks, outputPeaks, maximumGr, masterPeaks);
}

void MultiBandCompressor::publishMeters(const std::array<double, maxBands>& inputPeaks,
                                        const std::array<double, maxBands>& outputPeaks,
                                        const std::array<double, maxBands>& maximumGr,
                                        const std::array<double, 2>& masterPeaks) noexcept
{
    for (int band = 0; band < maxBands; ++band)
    {
        auto& meter = meters[static_cast<std::size_t>(band)];
        meter.inputDb.store(static_cast<float>(gainToDecibels(inputPeaks[static_cast<std::size_t>(band)], -100.0)), std::memory_order_relaxed);
        meter.outputDb.store(static_cast<float>(gainToDecibels(outputPeaks[static_cast<std::size_t>(band)], -100.0)), std::memory_order_relaxed);
        meter.gainReductionDb.store(static_cast<float>(maximumGr[static_cast<std::size_t>(band)]), std::memory_order_relaxed);
    }
    for (int channel = 0; channel < 2; ++channel)
        outputMeters[static_cast<std::size_t>(channel)].store(
            static_cast<float>(gainToDecibels(masterPeaks[static_cast<std::size_t>(channel)], -100.0)),
            std::memory_order_relaxed);
}

double MultiBandCompressor::getStaticOutputDb(const int band, const double inputDb) const noexcept
{
    if (band < 0 || band >= maxBands) return inputDb;
    const auto& p = currentParameters.bands[static_cast<std::size_t>(band)];
    return gainComputer.computeOutputDb(inputDb, p.thresholdDb, p.ratio, p.knee);
}

double MultiBandCompressor::getBandMagnitudeDb(const int band, const double frequency) const noexcept
{
    return crossover.getBandMagnitudeDb(band, frequency);
}

BandMeterSnapshot MultiBandCompressor::getBandMeter(const int band) const noexcept
{
    if (band < 0 || band >= maxBands) return {};
    const auto& meter = meters[static_cast<std::size_t>(band)];
    return { meter.inputDb.load(std::memory_order_relaxed),
             meter.outputDb.load(std::memory_order_relaxed),
             meter.gainReductionDb.load(std::memory_order_relaxed) };
}

std::array<float, 2> MultiBandCompressor::getOutputMeterDb() const noexcept
{
    return { outputMeters[0].load(std::memory_order_relaxed),
             outputMeters[1].load(std::memory_order_relaxed) };
}

} // namespace pontedsp::mc2000::dsp

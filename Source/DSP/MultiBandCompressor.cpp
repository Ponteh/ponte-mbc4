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
    detectorCrossover.prepare(sampleRate, maxChannels);
    for (auto& state : ballistics) state.prepare(sampleRate);
    for (auto& bite : biteProcessors) bite.prepare(sampleRate);
    inputGainCurrent = decibelsToGain(currentParameters.inputGainDb);
    outputGainCurrent = decibelsToGain(currentParameters.outputGainDb);
    for (int band = 0; band < maxBands; ++band)
    {
        const auto& p = currentParameters.bands[static_cast<std::size_t>(band)];
        bandGainCurrent[static_cast<std::size_t>(band)] = decibelsToGain(p.gainDb);
        inputMixCurrent[static_cast<std::size_t>(band)] = p.enabled ? 1.0 : 0.0;
        soloMixCurrent[static_cast<std::size_t>(band)] = 1.0;
    }
    crossoverCurrent = currentParameters.crossoverHz;
    crossover.setBandCount(currentParameters.numBands);
    crossover.setFrequencies(crossoverCurrent);
    detectorCrossover.setBandCount(currentParameters.numBands);
    detectorCrossover.setFrequencies(crossoverCurrent);
    crossoverUpdateCountdown = 0;
    reset();
}

void MultiBandCompressor::reset() noexcept
{
    crossover.reset();
    detectorCrossover.reset();
    for (auto& state : ballistics) state.reset();
    for (auto& bite : biteProcessors) bite.reset();
    for (auto& meter : meters)
    {
        meter.inputDb.store(-100.0f, std::memory_order_relaxed);
        meter.outputDb.store(-100.0f, std::memory_order_relaxed);
        meter.gainReductionDb.store(0.0f, std::memory_order_relaxed);
    }
    for (auto& meter : outputMeters) meter.store(-100.0f, std::memory_order_relaxed);
    discardPendingMeterPeaks();
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
        band.gainDb = clampFinite(band.gainDb, -24.0, 24.0, 0.0);
        band.thresholdDb = clampFinite(band.thresholdDb, -48.0, 0.0, 0.0);
        band.ratio = clampFinite(band.ratio, 1.0, 10.0, 1.0);
        band.knee = clampFinite(band.knee, -10.0, 15.0, 0.0);
        band.bite = clampFinite(band.bite, 1.0, 10.0, 1.0);
        band.attackMs = clampFinite(band.attackMs, 0.25, 250.0, 10.0);
        band.releaseMs = clampFinite(band.releaseMs, 25.0, 2500.0, 250.0);
        const auto mode = std::clamp(static_cast<int>(band.tcMode), 0, 2);
        band.tcMode = static_cast<TCMode>(mode);
    }
    crossover.setBandCount(currentParameters.numBands);
    detectorCrossover.setBandCount(currentParameters.numBands);
}

double MultiBandCompressor::smoothGain(const double current, const double target,
                                       const double coefficient) noexcept
{
    return coefficient * current + (1.0 - coefficient) * target;
}

void MultiBandCompressor::process(float** channels, const int channelCount,
                                  const int sampleCount) noexcept
{
    process(channels, channelCount, nullptr, 0, sampleCount);
}

void MultiBandCompressor::process(float** channels, const int channelCount,
                                  const float* const* detectorChannels,
                                  const int detectorChannelCount,
                                  const int sampleCount) noexcept
{
    if (channels == nullptr || channelCount <= 0 || sampleCount <= 0)
        return;
    const auto channelsToProcess = std::clamp(channelCount, 1, preparedChannels);
    for (int channel = 0; channel < channelsToProcess; ++channel)
        if (channels[channel] == nullptr) return;

    const auto useExternalDetector = detectorChannels != nullptr && detectorChannelCount > 0;
    const auto detectorChannelsToProcess = useExternalDetector
        ? std::clamp(detectorChannelCount, 1, maxChannels) : 0;
    if (useExternalDetector)
        for (int channel = 0; channel < detectorChannelsToProcess; ++channel)
            if (detectorChannels[channel] == nullptr) return;

    const auto bandsToProcess = currentParameters.numBands;
    const auto inputTarget = decibelsToGain(currentParameters.inputGainDb);
    const auto outputTarget = decibelsToGain(currentParameters.outputGainDb);
    const auto smoothing = std::exp(-1.0 / (sampleRate * 0.02));
    const auto routingSmoothing = std::exp(-1.0 / (sampleRate * 0.005));
    const auto crossoverSmoothing = 1.0 - std::exp(-1.0 / (sampleRate * 0.02));
    std::array<double, maxBands> bandGainTargets;
    for (int band = 0; band < maxBands; ++band)
        bandGainTargets[static_cast<std::size_t>(band)] = decibelsToGain(
            currentParameters.bands[static_cast<std::size_t>(band)].gainDb);
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

        for (int index = 0; index < 3; ++index)
            crossoverCurrent[static_cast<std::size_t>(index)] += crossoverSmoothing
                * (currentParameters.crossoverHz[static_cast<std::size_t>(index)]
                   - crossoverCurrent[static_cast<std::size_t>(index)]);
        crossoverCurrent[1] = std::max(crossoverCurrent[1], crossoverCurrent[0] + 1.0);
        crossoverCurrent[2] = std::max(crossoverCurrent[2], crossoverCurrent[1] + 1.0);
        if (--crossoverUpdateCountdown <= 0)
        {
            crossover.setFrequencies(crossoverCurrent);
            detectorCrossover.setFrequencies(crossoverCurrent);
            crossoverUpdateCountdown = 16;
        }

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

        std::array<std::array<double, maxChannels>, maxBands> detectorBandSamples {};
        if (useExternalDetector)
        {
            for (int channel = 0; channel < detectorChannelsToProcess; ++channel)
            {
                std::array<double, maxBands> splitBands {};
                const auto rawDetector = static_cast<double>(detectorChannels[channel][sample]);
                const auto finiteDetector = std::isfinite(rawDetector) ? rawDetector : 0.0;
                detectorCrossover.processSample(channel, finiteDetector, splitBands);
                for (int band = 0; band < bandsToProcess; ++band)
                    detectorBandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] =
                        splitBands[static_cast<std::size_t>(band)];
            }
        }

        for (int band = 0; band < bandsToProcess; ++band)
        {
            const auto& p = currentParameters.bands[static_cast<std::size_t>(band)];
            auto& inputMix = inputMixCurrent[static_cast<std::size_t>(band)];
            inputMix = smoothGain(inputMix, p.enabled ? 1.0 : 0.0, routingSmoothing);
            if (inputMix < 1.0e-9) inputMix = 0.0;
            if (inputMix > 1.0 - 1.0e-9) inputMix = 1.0;
            // IN gates the band input, including its detector. SOLO is an
            // independent output gate below; it must never reopen this input.
            for (int channel = 0; channel < maxChannels; ++channel)
            {
                bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] *= inputMix;
                detectorBandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] *= inputMix;
            }
            auto detector = 0.0;
            const auto detectorChannelTotal = useExternalDetector
                ? detectorChannelsToProcess : channelsToProcess;
            for (int channel = 0; channel < detectorChannelTotal; ++channel)
                detector = std::max(detector, std::abs(static_cast<double>(
                    (useExternalDetector ? detectorBandSamples : bandSamples)
                        [static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)])));
            inputPeaks[static_cast<std::size_t>(band)] = std::max(inputPeaks[static_cast<std::size_t>(band)], detector);

            auto targetGr = 0.0;
            if (detector > 1.0e-12)
                targetGr = gainComputer.computeGainReductionDb(gainToDecibels(detector),
                                                                p.thresholdDb, p.ratio, p.knee);
            auto gr = ballistics[static_cast<std::size_t>(band)].process(
                targetGr, detector, p.attackMs, p.releaseMs, p.tcMode, p.ratio);
            gr = biteProcessors[static_cast<std::size_t>(band)].process(gr, detector, p.bite);
            const auto appliedGr = gr;
            maximumGr[static_cast<std::size_t>(band)] = std::max(
                maximumGr[static_cast<std::size_t>(band)], appliedGr);
            auto& bandGain = bandGainCurrent[static_cast<std::size_t>(band)];
            bandGain = smoothGain(bandGain, bandGainTargets[static_cast<std::size_t>(band)], smoothing);
            auto& soloMix = soloMixCurrent[static_cast<std::size_t>(band)];
            soloMix = smoothGain(soloMix, !anySolo || p.solo ? 1.0 : 0.0, routingSmoothing);
            const auto appliedBandGain = bandGain * decibelsToGain(-appliedGr) * soloMix;

            for (int channel = 0; channel < channelsToProcess; ++channel)
            {
                auto& value = bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)];
                value *= appliedBandGain;
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
        auto& pending = pendingMeters[static_cast<std::size_t>(band)];
        pending.input.publish(meter.inputDb.load(std::memory_order_relaxed));
        pending.output.publish(meter.outputDb.load(std::memory_order_relaxed));
        pending.reduction.publish(static_cast<float>(maximumGr[static_cast<std::size_t>(band)]));
    }
    for (int channel = 0; channel < 2; ++channel)
    {
        outputMeters[static_cast<std::size_t>(channel)].store(
            static_cast<float>(gainToDecibels(masterPeaks[static_cast<std::size_t>(channel)], -100.0)),
            std::memory_order_relaxed);
        pendingOutputMeters[static_cast<std::size_t>(channel)].publish(
            outputMeters[static_cast<std::size_t>(channel)].load(std::memory_order_relaxed));
    }
}

BandMeterSnapshot MultiBandCompressor::consumeBandMeter(const int band) noexcept
{
    if (band < 0 || band >= maxBands) return {};
    auto& meter = pendingMeters[static_cast<std::size_t>(band)];
    return { meter.input.consume(), meter.output.consume(), meter.reduction.consume() };
}

std::array<float, 2> MultiBandCompressor::consumeOutputMeterDb() noexcept
{
    return { pendingOutputMeters[0].consume(), pendingOutputMeters[1].consume() };
}

void MultiBandCompressor::discardPendingMeterPeaks() noexcept
{
    for (auto& meter : pendingMeters)
    {
        meter.input.reset();
        meter.output.reset();
        meter.reduction.reset();
    }
    for (auto& meter : pendingOutputMeters) meter.reset();
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

#include "MultiBandCompressor.h"
#include "Db.h"

namespace pontedsp::mc2000::dsp {

void MultiBandCompressor::prepare(const double newSampleRate, const int maxBlockSize,
                                  const int numChannels)
{
    sampleRate = clampFinite(newSampleRate, 8000.0, 384000.0, 48000.0);
    activeCrossoverMode = currentParameters.crossoverMode;
    if (activeCrossoverMode == CrossoverMode::linearPhase) linearCrossover.prepare(sampleRate, currentParameters.crossoverHz); else linearCrossover.stopWorker();
    channelTransitionLength = std::max(1, static_cast<int>(sampleRate * .005));
    gainSmoothing = std::exp(-1.0 / (sampleRate * 0.02));
    routeSmoothing = std::exp(-1.0 / (sampleRate * 0.005));
    preparedBlockSize = std::max(1, maxBlockSize);
    preparedChannels = std::clamp(numChannels, 1, maxChannels);
    crossover.prepare(sampleRate, preparedChannels);
    detectorCrossover.prepare(sampleRate, maxChannels);
    for (auto& channel : ballistics)
        for (auto& state : channel) state.prepare(sampleRate);
    for (auto& channel : biteProcessors)
        for (auto& bite : channel) bite.prepare(sampleRate);
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
    publishIirResponse();
}

void MultiBandCompressor::reset() noexcept
{
    activity = Activity::active;
    crossover.reset();
    if (activeCrossoverMode == CrossoverMode::linearPhase) linearCrossover.reset();
    channelTransitionRemaining = 0; lastAppliedGr = {}; transitionFromGr = {};
    previousAudioChannels = previousDetectorChannels = 0;
    for (auto& channel : channelGr) for (auto& gr : channel) gr.store(0, std::memory_order_relaxed);
    detectorCrossover.reset();
    for (auto& channel : ballistics)
        for (auto& state : channel) state.reset();
    for (auto& channel : biteProcessors)
        for (auto& bite : channel) bite.reset();
    for (auto& meter : meters)
    {
        meter.inputDb.store(-100.0f, std::memory_order_relaxed);
        meter.outputDb.store(-100.0f, std::memory_order_relaxed);
        meter.gainReductionDb.store(0.0f, std::memory_order_relaxed);
        for (auto& value : meter.channelInputDb) value.store(-100.0f, std::memory_order_relaxed);
        for (auto& value : meter.channelOutputDb) value.store(-100.0f, std::memory_order_relaxed);
    }
    for (auto& meter : outputMeters) meter.store(-100.0f, std::memory_order_relaxed);
    discardPendingMeterPeaks();
}

void MultiBandCompressor::setParameters(const GlobalParameters& parameters) noexcept
{
    if (parameters == currentParameters) return;
    const auto previous = currentParameters;
    currentParameters = parameters;
    currentParameters.numBands = std::clamp(parameters.numBands, 2, maxBands);
    currentParameters.inputGainDb = clampFinite(parameters.inputGainDb, -24.0, 24.0, 0.0);
    currentParameters.outputGainDb = clampFinite(parameters.outputGainDb, -24.0, 24.0, 0.0);
    currentParameters.crossoverMode = static_cast<CrossoverMode>(
        std::clamp(static_cast<int>(parameters.crossoverMode), 0, 1));
    currentParameters.channelMode = static_cast<ChannelMode>(
        std::clamp(static_cast<int>(parameters.channelMode), 0, 1));
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
        band.sidechainSource = static_cast<SidechainSource>(std::clamp(static_cast<int>(band.sidechainSource), 0, 1));
    }
    if (previous != currentParameters) activity = Activity::active;
    if (previous.channelMode != currentParameters.channelMode)
    {
        transitionFromGr = lastAppliedGr;
        channelTransitionRemaining = channelTransitionLength;
        for (int band = 0; band < maxBands; ++band)
        {
            const auto b = static_cast<std::size_t>(band);
            if (currentParameters.channelMode == ChannelMode::dualMono)
            {
                ballistics[1][b] = ballistics[0][b];
                biteProcessors[1][b] = biteProcessors[0][b];
            }
            else if (lastAppliedGr[1][b] > lastAppliedGr[0][b])
            {
                ballistics[0][b] = ballistics[1][b];
                biteProcessors[0][b] = biteProcessors[1][b];
            }
        }
    }
    if (activeCrossoverMode == CrossoverMode::linearPhase)
        linearCrossover.requestFrequencies(currentParameters.crossoverHz);
    for (std::size_t i = 0; i < curves.size(); ++i)
    {
        curves[i].threshold.store(currentParameters.bands[i].thresholdDb, std::memory_order_relaxed);
        curves[i].ratio.store(currentParameters.bands[i].ratio, std::memory_order_relaxed);
        curves[i].knee.store(currentParameters.bands[i].knee, std::memory_order_relaxed);
    }
    inputGainTarget = decibelsToGain(currentParameters.inputGainDb);
    outputGainTarget = decibelsToGain(currentParameters.outputGainDb);
    for (std::size_t i = 0; i < bandGainTargets.size(); ++i)
        bandGainTargets[i] = decibelsToGain(currentParameters.bands[i].gainDb);
    crossover.setBandCount(currentParameters.numBands);
    detectorCrossover.setBandCount(currentParameters.numBands);
    publishIirResponse();
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

    const bool externalSelected = std::any_of(
        currentParameters.bands.begin(), currentParameters.bands.begin() + currentParameters.numBands,
        [](const BandParameters& band) { return band.sidechainSource == SidechainSource::all; });
    // An unused key must not wake NO bands or consume crossover work.
    const auto useExternalDetector = externalSelected && detectorChannels != nullptr && detectorChannelCount > 0;
    const auto detectorChannelsToProcess = useExternalDetector
        ? std::clamp(detectorChannelCount, 1, maxChannels) : 0;
    if (useExternalDetector)
        for (int channel = 0; channel < detectorChannelsToProcess; ++channel)
            if (detectorChannels[channel] == nullptr) return;

    // Inspect raw program AND key. No amplitude gate: even a subnormal wakes us.
    auto exactSilence = true;
    for (int channel = 0; exactSilence && channel < channelsToProcess; ++channel)
        for (int sample = 0; sample < sampleCount; ++sample)
            if (!exactlyZero(channels[channel][sample])) { exactSilence = false; break; }
    for (int channel = 0; exactSilence && channel < detectorChannelsToProcess; ++channel)
        for (int sample = 0; sample < sampleCount; ++sample)
            if (!exactlyZero(detectorChannels[channel][sample])) { exactSilence = false; break; }
    if (!napEnabled || !exactSilence || channelsToProcess != previousAudioChannels
        || detectorChannelsToProcess != previousDetectorChannels)
        activity = Activity::active;
    if (previousAudioChannels > channelsToProcess)
    {
        for (auto& state : ballistics[1]) state.reset();
        for (auto& state : biteProcessors[1]) state.reset();
    }
    if (detectorChannelsToProcess != previousDetectorChannels) detectorCrossover.reset();
    previousAudioChannels = channelsToProcess;
    previousDetectorChannels = detectorChannelsToProcess;
    if (activity == Activity::sleeping)
    {
        // Preserve the coefficient update phase, including across host block sizes.
        const auto countdown = std::max(1, crossoverUpdateCountdown);
        crossoverUpdateCountdown = ((countdown - 1 - sampleCount % 16) % 16 + 16) % 16 + 1;
        // Do not erase pending peaks not yet consumed by the GUI.
        for (auto& channel : channelGr) for (auto& gr : channel) gr.store(0, std::memory_order_relaxed);
        publishMeters({}, {}, {}, {}, {}, {});
        return;
    }

    const auto bandsToProcess = currentParameters.numBands;
    const auto inputTarget = inputGainTarget;
    const auto outputTarget = outputGainTarget;
    const auto smoothing = gainSmoothing;
    const auto routingSmoothing = routeSmoothing;
    const auto crossoverSmoothing = 1.0 - gainSmoothing;
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
    std::array<std::array<double, maxBands>, maxChannels> channelMaximumGr {};
    std::array<std::array<double, maxBands>, maxChannels> channelInputPeaks {}, channelOutputPeaks {};

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
        if (activeCrossoverMode == CrossoverMode::iir && --crossoverUpdateCountdown <= 0)
        {
            crossover.setFrequencies(crossoverCurrent);
            detectorCrossover.setFrequencies(crossoverCurrent);
            crossoverUpdateCountdown = 16;
        }

        std::array<std::array<double, maxChannels>, maxBands> bandSamples {}, detectorBandSamples {};
        std::array<double, 4> frame {};
        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            const double raw = channels[channel][sample];
            frame[static_cast<std::size_t>(channel)] = std::isfinite(raw) ? raw * inputGainCurrent : 0.0;
        }
        for (int channel = 0; channel < detectorChannelsToProcess; ++channel)
        {
            const double raw = detectorChannels[channel][sample];
            frame[static_cast<std::size_t>(channel + 2)] = std::isfinite(raw) ? raw : 0.0;
        }
        std::array<std::array<double, maxBands>, 4> split {};
        if (activeCrossoverMode == CrossoverMode::linearPhase)
            linearCrossover.processFrame(frame, ((1 << channelsToProcess) - 1) | (((1 << detectorChannelsToProcess) - 1) << 2), bandsToProcess, split);
        else
        {
            for (int channel = 0; channel < channelsToProcess; ++channel)
                crossover.processSample(channel, frame[static_cast<std::size_t>(channel)], split[static_cast<std::size_t>(channel)]);
            for (int channel = 0; channel < detectorChannelsToProcess; ++channel)
                detectorCrossover.processSample(channel, frame[static_cast<std::size_t>(channel + 2)], split[static_cast<std::size_t>(channel + 2)]);
        }
        for (int band = 0; band < bandsToProcess; ++band)
            for (int channel = 0; channel < maxChannels; ++channel)
            {
                bandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] = split[static_cast<std::size_t>(channel)][static_cast<std::size_t>(band)];
                detectorBandSamples[static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)] = split[static_cast<std::size_t>(channel + 2)][static_cast<std::size_t>(band)];
            }
        const auto transitionGr = [&] (int channel, int band, double gr)
        {
            const auto c = static_cast<std::size_t>(channel), b = static_cast<std::size_t>(band);
            if (channelTransitionRemaining > 0)
            {
                const double mix = 1.0 - double(channelTransitionRemaining) / channelTransitionLength;
                gr = transitionFromGr[c][b] * (1.0 - mix) + gr * mix;
            }
            lastAppliedGr[c][b] = gr;
            channelMaximumGr[c][b] = std::max(channelMaximumGr[c][b], gr);
            return gr;
        };
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
            const bool useBandExternalDetector = p.sidechainSource == SidechainSource::all;
            const auto detectorChannelTotal = useBandExternalDetector
                ? std::max(1, detectorChannelsToProcess) : channelsToProcess;
            auto& bandGain = bandGainCurrent[static_cast<std::size_t>(band)];
            bandGain = smoothGain(bandGain, bandGainTargets[static_cast<std::size_t>(band)], smoothing);
            auto& soloMix = soloMixCurrent[static_cast<std::size_t>(band)];
            soloMix = smoothGain(soloMix, !anySolo || p.solo ? 1.0 : 0.0, routingSmoothing);
            if (currentParameters.channelMode == ChannelMode::dualMono)
            {
                for (int channel = 0; channel < channelsToProcess; ++channel)
                {
                    const auto detectorChannel = useBandExternalDetector
                        ? (detectorChannelsToProcess == 1 ? 0 : std::min(channel, detectorChannelTotal - 1))
                        : channel;
                    const auto detector = std::abs(static_cast<double>(
                        (useBandExternalDetector ? detectorBandSamples : bandSamples)
                            [static_cast<std::size_t>(band)][static_cast<std::size_t>(detectorChannel)]));
                    auto& channelInput = channelInputPeaks[static_cast<std::size_t>(channel)][static_cast<std::size_t>(band)];
                    channelInput = std::max(channelInput, detector);
                    inputPeaks[static_cast<std::size_t>(band)] =
                        std::max(inputPeaks[static_cast<std::size_t>(band)], detector);
                    auto targetGr = 0.0;
                    if (detector > 1.0e-12)
                        targetGr = gainComputer.computeGainReductionDb(gainToDecibels(detector),
                                                                        p.thresholdDb, p.ratio, p.knee);
                    auto gr = ballistics[static_cast<std::size_t>(channel)]
                        [static_cast<std::size_t>(band)].process(
                            targetGr, detector, p.attackMs, p.releaseMs, p.tcMode, p.ratio);
                    gr = biteProcessors[static_cast<std::size_t>(channel)]
                        [static_cast<std::size_t>(band)].process(gr, detector, p.bite, p.tcMode);
                    gr = transitionGr(channel, band, gr);
                    maximumGr[static_cast<std::size_t>(band)] =
                        std::max(maximumGr[static_cast<std::size_t>(band)], gr);
                    auto& value = bandSamples[static_cast<std::size_t>(band)]
                        [static_cast<std::size_t>(channel)];
                    value *= bandGain * decibelsToGain(-gr) * soloMix;
                    const auto level = std::abs(value);
                    auto& channelOutput = channelOutputPeaks[static_cast<std::size_t>(channel)][static_cast<std::size_t>(band)];
                    channelOutput = std::max(channelOutput, level);
                    outputPeaks[static_cast<std::size_t>(band)] = std::max(
                        outputPeaks[static_cast<std::size_t>(band)], level);
                }
            }
            else
            {
                auto detector = 0.0;
                for (int channel = 0; channel < detectorChannelTotal; ++channel)
                {
                    const auto level = std::abs(static_cast<double>(
                        (useBandExternalDetector ? detectorBandSamples : bandSamples)
                            [static_cast<std::size_t>(band)][static_cast<std::size_t>(channel)]));
                    detector = std::max(detector, level);
                    if (channel < channelsToProcess)
                    {
                        auto& channelInput = channelInputPeaks[static_cast<std::size_t>(channel)][static_cast<std::size_t>(band)];
                        channelInput = std::max(channelInput, level);
                    }
                }
                // A mono external key feeds both program channels; a mono
                // program has one linked detector even with a stereo key.
                if (channelsToProcess == 1)
                    channelInputPeaks[0][static_cast<std::size_t>(band)] = std::max(channelInputPeaks[0][static_cast<std::size_t>(band)], detector);
                else if (useBandExternalDetector && detectorChannelTotal == 1)
                    channelInputPeaks[1][static_cast<std::size_t>(band)] = std::max(channelInputPeaks[1][static_cast<std::size_t>(band)], detector);
                inputPeaks[static_cast<std::size_t>(band)] =
                    std::max(inputPeaks[static_cast<std::size_t>(band)], detector);
                auto targetGr = 0.0;
                if (detector > 1.0e-12)
                    targetGr = gainComputer.computeGainReductionDb(gainToDecibels(detector),
                                                                    p.thresholdDb, p.ratio, p.knee);
                auto gr = ballistics[0][static_cast<std::size_t>(band)].process(
                    targetGr, detector, p.attackMs, p.releaseMs, p.tcMode, p.ratio);
                gr = biteProcessors[0][static_cast<std::size_t>(band)].process(
                    gr, detector, p.bite, p.tcMode);
                maximumGr[static_cast<std::size_t>(band)] =
                    std::max(maximumGr[static_cast<std::size_t>(band)], gr);
                const auto appliedBandGain = bandGain * decibelsToGain(-gr) * soloMix;
                for (int channel = 0; channel < channelsToProcess; ++channel)
                {
                    auto& value = bandSamples[static_cast<std::size_t>(band)]
                        [static_cast<std::size_t>(channel)];
                    const auto appliedGr = transitionGr(channel, band, gr);
                    value *= channelTransitionRemaining > 0 ? bandGain * decibelsToGain(-appliedGr) * soloMix : appliedBandGain;
                    const auto level = std::abs(value);
                    auto& channelOutput = channelOutputPeaks[static_cast<std::size_t>(channel)][static_cast<std::size_t>(band)];
                    channelOutput = std::max(channelOutput, level);
                    outputPeaks[static_cast<std::size_t>(band)] = std::max(
                        outputPeaks[static_cast<std::size_t>(band)], level);
                }
            }
        }

        if (channelTransitionRemaining > 0) --channelTransitionRemaining;
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

    if (napEnabled && exactSilence)
    {
        activity = Activity::draining;
        auto quiet = (activeCrossoverMode == CrossoverMode::linearPhase ? linearCrossover.isQuiet() : (crossover.isQuiet(channelsToProcess)
            && (!useExternalDetector || detectorCrossover.isQuiet(detectorChannelsToProcess))))
            && channelTransitionRemaining == 0
            && exactlyEqual(smoothGain(inputGainCurrent, inputTarget, smoothing), inputGainCurrent)
            && exactlyEqual(smoothGain(outputGainCurrent, outputTarget, smoothing), outputGainCurrent);
        for (std::size_t i = 0; quiet && i < crossoverCurrent.size(); ++i)
            quiet = exactlyEqual(crossoverCurrent[i] + crossoverSmoothing
                * (currentParameters.crossoverHz[i] - crossoverCurrent[i]),
                crossoverCurrent[i]);
        for (int band = 0; quiet && band < bandsToProcess; ++band)
        {
            const auto i = static_cast<std::size_t>(band);
            const auto& p = currentParameters.bands[i];
            const auto soloTarget = !anySolo || p.solo ? 1.0 : 0.0;
            quiet = true;
            for (int channel = 0; channel < (currentParameters.channelMode == ChannelMode::dualMono ? channelsToProcess : 1); ++channel)
                quiet = quiet && ballistics[static_cast<std::size_t>(channel)][i].isQuiet()
                    && biteProcessors[static_cast<std::size_t>(channel)][i].isQuiet();
            quiet = quiet
                && exactlyEqual(smoothGain(bandGainCurrent[i], bandGainTargets[i], smoothing),
                                bandGainCurrent[i])
                && exactlyEqual(inputMixCurrent[i], p.enabled ? 1.0 : 0.0)
                && (exactlyEqual(smoothGain(soloMixCurrent[i], soloTarget, routingSmoothing),
                                 soloMixCurrent[i])
                    || (exactlyZero(soloTarget) && soloMixCurrent[i] < 1.0e-24));
        }
        // Freeze only exhausted active states. Dormant bands/filters retain
        // exactly the history they would have kept without nap.
        if (quiet) activity = Activity::sleeping;
    }
    for (std::size_t c = 0; c < maxChannels; ++c)
        for (std::size_t b = 0; b < maxBands; ++b)
        {
            channelGr[c][b].store(static_cast<float>(channelMaximumGr[c][b]), std::memory_order_relaxed);
            pendingChannelGr[c][b].value.publish(static_cast<float>(channelMaximumGr[c][b]));
        }
    publishIirResponse();
    publishMeters(inputPeaks, outputPeaks, maximumGr, masterPeaks, channelInputPeaks, channelOutputPeaks);
}

void MultiBandCompressor::publishMeters(const std::array<double, maxBands>& inputPeaks,
                                        const std::array<double, maxBands>& outputPeaks,
                                        const std::array<double, maxBands>& maximumGr,
                                        const std::array<double, 2>& masterPeaks,
                                        const std::array<std::array<double, maxBands>, maxChannels>& channelInputPeaks,
                                        const std::array<std::array<double, maxBands>, maxChannels>& channelOutputPeaks) noexcept
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
        for (std::size_t channel = 0; channel < maxChannels; ++channel)
        {
            const auto b = static_cast<std::size_t>(band);
            const auto input = static_cast<float>(gainToDecibels(channelInputPeaks[channel][b], -100.0));
            const auto output = static_cast<float>(gainToDecibels(channelOutputPeaks[channel][b], -100.0));
            meter.channelInputDb[channel].store(input, std::memory_order_relaxed);
            meter.channelOutputDb[channel].store(output, std::memory_order_relaxed);
            pending.channelInput[channel].publish(input);
            pending.channelOutput[channel].publish(output);
        }
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
    return { meter.input.consume(), meter.output.consume(), meter.reduction.consume(),
             { pendingChannelGr[0][static_cast<std::size_t>(band)].value.consume(), pendingChannelGr[1][static_cast<std::size_t>(band)].value.consume() },
             { meter.channelInput[0].consume(), meter.channelInput[1].consume() },
             { meter.channelOutput[0].consume(), meter.channelOutput[1].consume() } };
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
        for (auto& channel : meter.channelInput) channel.reset();
        for (auto& channel : meter.channelOutput) channel.reset();
    }
    for (auto& meter : pendingOutputMeters) meter.reset();
    for (auto& channel : pendingChannelGr) for (auto& meter : channel) meter.value.reset();
}

double MultiBandCompressor::getStaticOutputDb(const int band, const double inputDb) const noexcept
{
    if (band < 0 || band >= maxBands) return inputDb;
    const auto p = getStaticCurveParameters(band);
    return gainComputer.computeOutputDb(inputDb, p[0], p[1], p[2]);
}

std::array<double, 3> MultiBandCompressor::getStaticCurveParameters(const int band) const noexcept
{
    if (band < 0 || band >= maxBands) return { 0.0, 1.0, 0.0 };
    const auto& curve = curves[static_cast<std::size_t>(band)];
    return { curve.threshold.load(std::memory_order_relaxed),
             curve.ratio.load(std::memory_order_relaxed), curve.knee.load(std::memory_order_relaxed) };
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
             meter.gainReductionDb.load(std::memory_order_relaxed),
             { channelGr[0][static_cast<std::size_t>(band)].load(std::memory_order_relaxed), channelGr[1][static_cast<std::size_t>(band)].load(std::memory_order_relaxed) },
             { meter.channelInputDb[0].load(std::memory_order_relaxed), meter.channelInputDb[1].load(std::memory_order_relaxed) },
             { meter.channelOutputDb[0].load(std::memory_order_relaxed), meter.channelOutputDb[1].load(std::memory_order_relaxed) } };
}

std::array<float, 2> MultiBandCompressor::getOutputMeterDb() const noexcept
{
    return { outputMeters[0].load(std::memory_order_relaxed),
             outputMeters[1].load(std::memory_order_relaxed) };
}

} // namespace pontedsp::mc2000::dsp

namespace pontedsp::mc2000::dsp {
void MultiBandCompressor::publishIirResponse() noexcept {
    if(lastIirRevision==crossover.responseRevision())return;
    lastIirRevision=crossover.responseRevision();
    const auto r=crossover.responseSnapshot();
    iirResponseVersion.fetch_add(1,std::memory_order_seq_cst);
    iirResponseData[0].store(r.sampleRate,std::memory_order_seq_cst);
    for(std::size_t i=0;i<3;++i)iirResponseData[i+1].store(r.frequencies[i],std::memory_order_seq_cst);
    std::size_t at=4;
    for(const auto& section:r.sections)for(const auto& c:section)
        for(double value:{c.b0,c.b1,c.b2,c.a1,c.a2})iirResponseData[at++].store(value,std::memory_order_seq_cst);
    iirResponseBands.store(r.bands,std::memory_order_seq_cst);
    iirResponseVersion.fetch_add(1,std::memory_order_seq_cst);
}
bool MultiBandCompressor::copyIirResponse(CrossoverResponse& output,unsigned& version) const noexcept {
    for(int attempt=0;attempt<3;++attempt){
        const auto before=iirResponseVersion.load(std::memory_order_seq_cst);
        if(before==0 || (before&1u))continue;
        CrossoverResponse r;r.sampleRate=iirResponseData[0].load(std::memory_order_seq_cst);
        for(std::size_t i=0;i<3;++i)r.frequencies[i]=iirResponseData[i+1].load(std::memory_order_seq_cst);
        std::size_t at=4;
        for(auto& section:r.sections)for(auto& c:section){
            c.b0=iirResponseData[at++].load(std::memory_order_seq_cst);c.b1=iirResponseData[at++].load(std::memory_order_seq_cst);c.b2=iirResponseData[at++].load(std::memory_order_seq_cst);c.a1=iirResponseData[at++].load(std::memory_order_seq_cst);c.a2=iirResponseData[at++].load(std::memory_order_seq_cst);
        }
        r.bands=iirResponseBands.load(std::memory_order_seq_cst);
        if(before!=iirResponseVersion.load(std::memory_order_seq_cst))continue;
        output=r;version=before;return true;
    }return false;
}
}
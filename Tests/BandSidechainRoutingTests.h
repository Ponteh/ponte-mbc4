#pragma once
#include <memory>
namespace {
void testPerBandSidechainRouting()
{
    using namespace pontedsp::mc2000::dsp;
    constexpr int blockSize = 257;
    for (auto backend : {CrossoverMode::iir, CrossoverMode::linearPhase})
        for (auto channels : {ChannelMode::stereo, ChannelMode::dualMono})
        {
            GlobalParameters parameters;
            parameters.crossoverMode = backend;
            parameters.channelMode = channels;
            for (auto& band : parameters.bands)
            {
                band.thresholdDb = -30;
                band.ratio = 4;
                band.attackMs = .25;
                band.releaseMs = 25;
            }
            auto internal = std::make_unique<MultiBandCompressor>();
            auto external = std::make_unique<MultiBandCompressor>();
            auto mixed = std::make_unique<MultiBandCompressor>();
            internal->setParameters(parameters);
            auto all = parameters;
            for (auto& band : all.bands) band.sidechainSource = SidechainSource::all;
            external->setParameters(all);
            auto routing = parameters;
            routing.bands[0].sidechainSource = routing.bands[2].sidechainSource = SidechainSource::all;
            mixed->setParameters(routing);
            for (auto* engine : {internal.get(), external.get(), mixed.get()})
                engine->prepare(48000, blockSize, 2);
            std::array<float, blockSize> il{}, ir{}, el{}, er{}, ml{}, mr{}, kl{}, kr{};
            float* ia[]{il.data(), ir.data()};
            float* ea[]{el.data(), er.data()};
            float* ma[]{ml.data(), mr.data()};
            const float* key[]{kl.data(), kr.data()};
            bool sawCompression = false, sawIndependentChannels = false;
            for (int offset = 0; offset < 36000; offset += blockSize)
            {
                for (int sample = 0; sample < blockSize; ++sample)
                {
                    double tone = 0;
                    for (double frequency : {50., 400., 3200., 16000.})
                        tone += .15 * std::sin(2 * std::numbers::pi * frequency * (offset + sample) / 48000);
                    il[sample] = el[sample] = ml[sample] = float(.025 * tone);
                    ir[sample] = er[sample] = mr[sample] = float(.008 * tone);
                    // The DAW sums the sends as AUDIO, before the key crossover.
                    kl[sample] = float(tone);
                    kr[sample] = float(.25 * tone);
                }
                internal->process(ia, 2, blockSize);
                external->process(ea, 2, key, 2, blockSize);
                mixed->process(ma, 2, key, 2, blockSize);
                for (int band = 0; band < 4; ++band)
                {
                    const auto actual = mixed->getBandMeter(band);
                    const auto expected = (band % 2 == 0 ? external : internal)->getBandMeter(band);
                    for (int channel = 0; channel < 2; ++channel)
                    {
                        expectNear(actual.channelInputDb[channel], expected.channelInputDb[channel], 1.e-5,
                                   "Each band meters only its selected NO/ALL detector");
                        expectNear(actual.channelGainReductionDb[channel], expected.channelGainReductionDb[channel], 1.e-5,
                                   "Per-band NO/ALL GR equals the independently routed reference");
                        expectNear(actual.channelOutputDb[channel], expected.channelOutputDb[channel], 1.e-5,
                                   "Per-band output gain uses the selected detector");
                    }
                    sawCompression |= actual.gainReductionDb > 2;
                    sawIndependentChannels |= std::abs(actual.channelGainReductionDb[0] - actual.channelGainReductionDb[1]) > 1;
                }
            }
            expect(sawCompression, "An ALL band responds to key audio in both crossover modes");
            expect(channels != ChannelMode::dualMono || sawIndependentChannels,
                   "ALL preserves independent stereo key channels in Dual Mono");

            // NO ignores even a connected loud key, including during drained silence.
            internal->reset(); mixed->reset();
            mixed->setParameters(parameters);
            for (int iteration = 0; iteration < 12; ++iteration)
            {
                il.fill(0); ir.fill(0); ml.fill(0); mr.fill(0); kl.fill(.9f); kr.fill(-.9f);
                internal->process(ia, 2, blockSize);
                mixed->process(ma, 2, key, 2, blockSize);
                expect(il == ml && ir == mr, "Default NO output is unchanged by external key audio");
            }
            expect(mixed->getActivity() == MultiBandCompressor::Activity::sleeping,
                   "An unused external key cannot prevent NO from sleeping");

            // No external bus with ALL must mean silence, not internal fallback.
            external->reset();
            for (int iteration = 0; iteration < 110; ++iteration)
            {
                el.fill(.5f); er.fill(.5f);
                external->process(ea, 2, blockSize);
            }
            for (int band = 0; band < 4; ++band)
                expect(external->getBandMeter(band).inputDb == -100
                       && external->getBandMeter(band).gainReductionDb == 0,
                       "Disconnected ALL uses a silent key, never the internal detector");
        }
}
} // namespace

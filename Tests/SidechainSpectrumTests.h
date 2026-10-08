#pragma once
namespace {
void testSidechainSpectrum()
{
    using namespace pontedsp::mc2000;
    constexpr int window = CrossoverPlot::spectrumSize;
    juce::MidiBuffer midi;
    for (int backend : {0, 1}) for (int keyChannels : {1, 2}) for (int mode : {0, 1})
    {
        PonteMC2000AudioProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(1, keyChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        expect(p.setBusesLayout(layout), "Spectrum accepts mono/stereo external key buses");
        set(p, parameters::crossoverMode, float(backend));
        set(p, parameters::channelMode, float(mode));
        set(p, parameters::bandId(0, "sidechainSource"), 1);
        p.prepareToPlay(48000, 512);
        CrossoverPlot plot(p); plot.setSize(570, 200);
        juce::AudioBuffer<float> audio(2 + keyChannels, 512);
        int position = 0;
        const auto send = [&](int samples, bool silentKey = false)
        {
            for (int remaining = samples; remaining > 0;)
            {
                const int count = std::min(remaining, 512);
                audio.setSize(2 + keyChannels, count, false, false, true);
                for (int i = 0; i < count; ++i)
                {
                    const double phase = 2 * juce::MathConstants<double>::pi * (position + i) / window;
                    const float program = float(.125 * std::sin(16 * phase));
                    const float key = silentKey ? 0.f : float(.5 * std::sin(2 * phase) + .125 * std::sin(128 * phase));
                    audio.setSample(0, i, program); audio.setSample(1, i, -program);
                    audio.setSample(2, i, key);
                    if (keyChannels == 2) audio.setSample(3, i, -key);
                }
                p.processBlock(audio, midi);
                position += count; remaining -= count;
            }
            plot.updateSpectrum(double(samples) / p.getProcessingSampleRate());
        };
        send(window);
        std::cout << "Sidechain spectrum: backend=" << backend << " mode=" << mode
                  << " keyChannels=" << keyChannels << " lowPeakDb=" << plot.displayedSidechainSpectrumDb(2)
                  << " keyFFTs=" << plot.performedSidechainFftTransforms() << '\n';
        const double lowHz = 2.0 * 48000 / window;
        const double warped = std::tan(juce::MathConstants<double>::pi * lowHz / 48000)
                            / std::tan(juce::MathConstants<double>::pi * 100 / 48000);
        const double expected = -6.020599913 + (backend == 0 ? -20 * std::log10(1 + std::pow(warped, 4)) : 0);
        expect(std::abs(plot.displayedSidechainSpectrumDb(2) - expected) < .05,
               "Key FFT shows the real source level with the active low-band response");
        expect(std::abs(plot.displayedSpectrumDb(16) + 18.06179974) < .04,
               "Program spectrum retains its independent level beside a different external key");
        expect(plot.sidechainVisibleAtFrequency(lowHz) && !plot.sidechainVisibleAtFrequency(375),
               "Only band 1 shows the key when band 2 is NO");
        expect(plot.performedSidechainFftTransforms() == std::uint64_t(keyChannels),
               "Mono key uses one FFT; anti-phase stereo uses two, independent of channel mode");
        const auto beforePaint = plot.performedFftTransforms();
        const auto keyPeak = plot.displayedSidechainSpectrumDb(2);
        for (int i = 0; i < 3; ++i) plot.createComponentSnapshot(plot.getLocalBounds());
        expect(plot.performedFftTransforms() == beforePaint && plot.displayedSidechainSpectrumDb(2) == keyPeak,
               "Painting never consumes audio, computes FFTs or advances key ballistics");

        // A pending window from the old route must not appear under a new selection.
        for (int i = 0; i < 512; ++i)
        {
            audio.setSample(0, i, 0); audio.setSample(1, i, 0); audio.setSample(2, i, .8f);
            if (keyChannels == 2) audio.setSample(3, i, -.8f);
        }
        p.processBlock(audio, midi);
        set(p, parameters::bandId(0, "sidechainSource"), 0);
        set(p, parameters::bandId(2, "sidechainSource"), 1);
        const auto oldTransforms = plot.performedSidechainFftTransforms();
        plot.updateSpectrum(.033);
        expect(!plot.sidechainVisibleAtFrequency(3000) && plot.performedSidechainFftTransforms() == oldTransforms,
               "Queued samples from an old routing cannot populate a newly selected band");
        send(window);
        expect(plot.sidechainVisibleAtFrequency(3000) && !plot.sidechainVisibleAtFrequency(lowHz)
               && !plot.sidechainVisibleAtFrequency(500),
               "Switching ALL to band 3 moves the overlay without painting NO bands");
        set(p, parameters::bandId(0, "sidechainSource"), 1);
        send(window);
        expect(plot.sidechainVisibleAtFrequency(lowHz) && plot.sidechainVisibleAtFrequency(3000)
               && !plot.sidechainVisibleAtFrequency(500),
               "Nonadjacent ALL bands remain separate with no bridge through a NO band");
        set(p, parameters::bandId(3, "solo"), 1);
        send(window);
        expect(plot.sidechainVisibleAtFrequency(lowHz) && plot.sidechainVisibleAtFrequency(3000),
               "SOLO selects outputs and does not crop the input/key analyzers");

        for (int band = 0; band < 4; ++band) set(p, parameters::bandId(band, "sidechainSource"), 1);
        const auto beforeAll = plot.performedSidechainFftTransforms();
        send(window);
        expect(plot.performedSidechainFftTransforms() - beforeAll == std::uint64_t(keyChannels),
               "Four ALL bands still share a single source analysis");
        if (backend == 0 && mode == 0 && keyChannels == 2)
        {
            // Capture the requested mixed-route look with the original input trace.
            set(p, parameters::bandId(1, "sidechainSource"), 0);
            set(p, parameters::bandId(3, "sidechainSource"), 0);
            plot.setSpectrumActive(false);
            auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
            offscreenPeer(*editor);
            send(window * 2); pump(80);
            const auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                .getParentDirectory().getChildFile("MC2000_UI_sidechain_spectrum.png");
            juce::FileOutputStream stream(file);
            if (stream.openedOk()) {stream.setPosition(0); stream.truncate();}
            juce::PNGImageFormat png;
            expect(stream.openedOk() && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), stream),
                   "Mixed NO/ALL sidechain spectrum renders at the minimum editor size");
            editor.reset();
            plot.setSpectrumActive(true);
        }

        for (int band = 0; band < 4; ++band) set(p, parameters::bandId(band, "sidechainSource"), 0);
        const auto beforeNo = plot.performedSidechainFftTransforms();
        send(window * 2);
        expect(plot.performedSidechainFftTransforms() == beforeNo
               && !plot.sidechainVisibleAtFrequency(lowHz) && !plot.sidechainVisibleAtFrequency(3000),
               "All NO bands stop key FFT work and immediately remove its overlay");
        set(p, parameters::bandId(0, "sidechainSource"), 1);
        set(p, parameters::bandId(0, "enabled"), 0);
        send(window);
        expect(plot.performedSidechainFftTransforms() == beforeNo && !plot.sidechainVisibleAtFrequency(lowHz),
               "Disabled IN bands do not capture or analyze unused key audio");
        set(p, parameters::bandId(0, "enabled"), 1);
        plot.setSpectrumActive(false);
        const auto beforeHidden = plot.performedFftTransforms();
        send(window);
        std::array<PonteMC2000AudioProcessor::SpectrumSample, 32> pending;
        bool gap = false;
        expect(p.popSpectrumSamples(pending.data(), 32, gap) == 0
               && plot.performedFftTransforms() == beforeHidden,
               "Hidden analyzer produces no FIFO traffic or FFT work even with a live key");
        plot.setSpectrumActive(true);
        send(window);
        expect(plot.sidechainVisibleAtFrequency(lowHz), "Reopening waits for a fresh key window then resumes");
        plot.updateSpectrum(12);
        expect(plot.displayedSidechainSpectrumDb(2) < -99, "Key spectrum decays when DAW callbacks stop");

        layout.inputBuses.set(1, juce::AudioChannelSet::disabled());
        expect(p.setBusesLayout(layout), "Key bus can be disconnected");
        p.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> disconnected(2, window); disconnected.clear();
        p.processBlock(disconnected, midi); plot.updateSpectrum(.05);
        expect(!plot.sidechainVisibleAtFrequency(lowHz), "Unavailable ALL key removes the overlay");
    }
}
} // namespace

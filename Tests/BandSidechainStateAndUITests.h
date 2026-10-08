#pragma once
namespace {
void testBandSidechainStateAndUI()
{
    using namespace pontedsp::mc2000;
    PonteMC2000AudioProcessor p;
    p.prepareToPlay(48000, 257);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
    offscreenPeer(*editor);
    for (int band = 0; band < 4; ++band)
    {
        const auto id = parameters::bandId(band, "sidechainSource");
        expect(p.state.getRawParameterValue(id)->load() == 0, "Every band defaults to NO");
        auto* selector = find<juce::ComboBox>(*editor, id);
        expect(selector && selector->getText() == "NO" && selector->getNumItems() == 2,
               "Every band exposes the real NO/ALL sidechain choices");
        expect(dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[50 + band])->paramID == id,
               "Sidechain parameters append after all 50 existing automation indices");
    }
    for (const auto size : {juce::Point<int>(1100, 738), juce::Point<int>(1330, 950), juce::Point<int>(1600, 1100)})
        for (int count : {2, 3, 4})
        {
            editor->setSize(size.x, size.y);
            set(p, parameters::bandCount, float(count - 2)); pump(40);
            for (int band = 0; band < count; ++band)
            {
                auto* sc = find<juce::ComboBox>(*editor, parameters::bandId(band, "sidechainSource"));
                auto* strip = sc ? sc->getParentComponent() : nullptr;
                juce::ComboBox* algorithm = nullptr;
                if (strip) for (auto* child : strip->getChildren())
                    if (auto* combo = dynamic_cast<juce::ComboBox*>(child); combo && combo != sc) algorithm = combo;
                expect(sc && strip && algorithm && strip->getLocalBounds().contains(sc->getBounds())
                       && sc->getWidth() >= 60 && sc->getHeight() >= 18
                       && algorithm->getY() == sc->getY() && algorithm->getRight() < sc->getX(),
                       "ALGORITHM and SIDECHAIN fit beside each other at every editor size and band count");
            }
        }
    auto* first = find<juce::ComboBox>(*editor, parameters::bandId(0, "sidechainSource"));
    if (first) first->setSelectedId(2, juce::sendNotificationSync);
    set(p, parameters::bandId(2, "sidechainSource"), 1);
    set(p, parameters::linkMaster, 1);
    parameters::SnapshotReader reader(p.state); parameters::LinkRuntime runtime;
    const auto snapshot = reader.read(runtime);
    expect(snapshot.bands[0].sidechainSource == dsp::SidechainSource::all
           && snapshot.bands[1].sidechainSource == dsp::SidechainSource::internal
           && snapshot.bands[2].sidechainSource == dsp::SidechainSource::all,
           "LINK never copies per-band sidechain routing");
    juce::MemoryBlock state; p.getStateInformation(state);
    PonteMC2000AudioProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize()));
    expect(restored.state.getRawParameterValue(parameters::bandId(0, "sidechainSource"))->load() == 1
           && restored.state.getRawParameterValue(parameters::bandId(1, "sidechainSource"))->load() == 0,
           "Mixed NO/ALL selections survive project recall");
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
    auto legacy = juce::ValueTree::fromXml(*xml); legacy.setProperty("schemaVersion", 3, nullptr);
    for (int band = 0; band < 4; ++band)
    {
        legacy.removeChild(legacy.getChildWithProperty("id", parameters::bandId(band, "sidechainSource")), nullptr);
        set(restored, parameters::bandId(band, "sidechainSource"), 1);
    }
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), state);
    restored.setStateInformation(state.getData(), int(state.getSize()));
    for (int band = 0; band < 4; ++band)
        expect(restored.state.getRawParameterValue(parameters::bandId(band, "sidechainSource"))->load() == 0,
               "Legacy states loaded over ALL restore default NO explicitly");
    auto malformed = legacy;
    juce::ValueTree route("PARAM");
    route.setProperty("id", parameters::bandId(0, "sidechainSource"), nullptr);
    route.setProperty("value", 12345, nullptr); malformed.addChild(route, -1, nullptr);
    juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), state);
    restored.setStateInformation(state.getData(), int(state.getSize()));
    expect(restored.state.getRawParameterValue(parameters::bandId(0, "sidechainSource"))->load() == 1,
           "Out-of-range recalled sidechain values clamp to a valid route");

    // Equal and opposite sends cancel BEFORE detection; taking max envelopes is incorrect.
    auto layout = p.getBusesLayout(); layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    expect(p.setBusesLayout(layout), "Standard stereo sidechain remains supported");
    for (int band = 0; band < 4; ++band)
    {
        set(p, parameters::bandId(band, "sidechainSource"), 1);
        set(p, parameters::bandId(band, "ratio"), 4);
        set(p, parameters::bandId(band, "thresholdDb"), -30);
    }
    p.prepareToPlay(48000, 257);
    juce::AudioBuffer<float> audio(4, 257); juce::MidiBuffer midi;
    for (int iteration = 0; iteration < 30; ++iteration)
    {
        for (int sample = 0; sample < 257; ++sample)
        {
            const float a = float(.5 * std::sin((iteration * 257 + sample) * .07));
            audio.setSample(0, sample, .2f); audio.setSample(1, sample, .2f);
            audio.setSample(2, sample, a + -a); audio.setSample(3, sample, a + -a);
        }
        p.processBlock(audio, midi);
    }
    expect(p.getEngine().getBandMeter(0).gainReductionDb == 0,
           "ALL accepts the DAW audio sum with its phase cancellations intact");
    set(p, parameters::bandId(0, "sidechainSource"), 0);
    set(p, parameters::bandId(1, "sidechainSource"), 0);
    editor->setSize(1100, 738); set(p, parameters::bandCount, 2); pump(80);
    const auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory().getChildFile("MC2000_UI_band_sidechain.png");
    juce::FileOutputStream stream(file); if (stream.openedOk()) {stream.setPosition(0); stream.truncate();}
    juce::PNGImageFormat png;
    expect(stream.openedOk() && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), stream),
           "Sidechain selectors render in the minimum mockup layout");
}
} // namespace

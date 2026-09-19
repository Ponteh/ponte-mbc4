#include "PluginEditor.h"
#include "UI/FocusTiming.h"
#include <iostream>

namespace {
int failures {};
void expect(bool ok, const char* message)
{
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void pump(int ms = 60) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

void offscreenPeer(juce::Component& c)
{
    c.setTopLeftPosition(-20000, -20000);
    c.addToDesktop(juce::ComponentPeer::windowIsTemporary
        | juce::ComponentPeer::windowIgnoresMouseClicks
        | juce::ComponentPeer::windowIgnoresKeyPresses);
    c.setVisible(true);
}

template <typename T>
T* find(juce::Component& parent, const juce::String& id = {})
{
    if (auto* c = dynamic_cast<T*>(&parent); c != nullptr && (id.isEmpty() || c->getComponentID() == id)) return c;
    for (auto* c : parent.getChildren()) if (auto* match = find<T>(*c, id)) return match;
    return nullptr;
}

void set(PonteMC2000AudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void testTiming()
{
    pontedsp::gui::FocusTiming focus;
    expect(focus.update(1, 0) == 0, "initial hover waits");
    expect(focus.update(1, 29) == 0, "no focus before 30 ms");
    expect(focus.update(1, 30) == 1, "focus at 30 ms");
    expect(focus.update(2, 31) == 2, "handoff immediately on next hover");
    expect(focus.update(0, 130) == 2, "focus retained at 99 ms inactivity");
    expect(focus.update(0, 131) == 0, "focus removed at 100 ms inactivity");
    expect(focus.update(3, 132, true) == 3, "click acquires immediately from idle");
    expect(focus.update(3, 900) == 3, "stationary hover stays active");
    focus.update(0, 1000);
    focus.update(1, 1010);
    focus.update(0, 1020);
    expect(focus.update(1, 1025) == 0, "leaving cancels a pending hover");
    expect(focus.update(1, 1055) == 1, "re-entry starts a fresh 30 ms dwell");
}

void testFocusController()
{
    juce::Component root, knob, alias;
    juce::ComboBox selector;
    juce::Label field;
    root.setBounds(0, 0, 500, 300);
    root.setVisible(true);
    offscreenPeer(root);
    for (auto* c : std::array<juce::Component*, 4>{ &knob, &alias, &selector, &field }) root.addAndMakeVisible(c);
    bool knobOn {}, selectorOn {}, fieldOn {};
    pontedsp::gui::ControlFocus focus(root);
    focus.add(knob, [&](bool on) { knobOn = on; }, &alias);
    focus.add(selector, [&](bool on) { selectorOn = on; });
    focus.add(field, [&](bool on) { fieldOn = on; });
    focus.pointerActivity(&knob, true);
    expect(knobOn && !selectorOn && !fieldOn, "only clicked knob active");
    focus.pointerActivity(&alias);
    expect(knobOn, "popup value aliases its knob");
    focus.pointerActivity(&selector);
    expect(!knobOn && selectorOn && !fieldOn, "hover transfers knob focus to selector immediately");
    focus.pointerActivity(&field);
    expect(!knobOn && !selectorOn && fieldOn, "value field shares the same focus controller");
    field.setVisible(false);
    pump(130);
    expect(!fieldOn, "hidden control cannot retain visual focus");
}

void testEditorAndSolo()
{
    using namespace pontedsp::mc2000::parameters;
    juce::Component host;
    host.setSize(1600, 1100);
    offscreenPeer(host);
    PonteMC2000AudioProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    host.addAndMakeVisible(*editor);
    expect(editor->getWidth() == 1100 && editor->getHeight() == 738, "new instance opens at four-band minimum");
    auto* meter = find<OutputMeter>(*editor);
    auto* plot = find<CompressionPlot>(*editor);
    expect(meter && plot && meter->getX() == plot->getX() && meter->getWidth() == plot->getWidth(),
           "MAIN OUTPUT matches STATIC I/O width and left edge");
    expect(meter && plot && meter->getBottom() <= plot->getY(), "main meter stays above plot");
    auto* crossover = find<CrossoverPlot>(*editor);
    auto* caption = find<juce::Label>(*editor, "crossoverCaption");
    auto* helpHeader = find<ContextHeader>(*editor);
    expect(crossover && caption && caption->getX() == crossover->getX(),
           "CROSSOVER starts at the left edge of the band display");
    expect(helpHeader && caption && helpHeader->getRight() <= caption->getX(), "expanded help does not overlap CROSSOVER");
    const std::function<void(juce::Component&)> checkHelp = [&](juce::Component& c)
    {
        const auto text = c.getProperties()["mbc4ContextHelp"].toString();
        if (helpHeader && text.isNotEmpty())
        {
            juce::AttributedString content(text);
            content.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            juce::TextLayout layout;
            layout.createLayout(content, static_cast<float>(helpHeader->getWidth() - 6));
            expect(layout.getHeight() <= helpHeader->getHeight() - 4, "complete contextual help fits at minimum editor size");
        }
        for (auto* child : c.getChildren()) checkHelp(*child);
    };
    checkHelp(*editor);
    editor->setSize(1330, 950);
    for (const auto count : { 2, 3, 4, 2, 4 })
    {
        set(p, bandCount, static_cast<float>(count - 2)); pump();
        expect(editor->getWidth() == 1330 && editor->getHeight() == 950, "band-count changes preserve resize");
    }
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "enabled"), b == 1 || b == 2 ? 1.0f : 0.0f);
    pump();
    auto clickSolo = [&](int b)
    {
        auto* button = find<juce::TextButton>(*editor, bandId(b, "solo"));
        expect(button != nullptr, "SOLO button exists");
        if (button) button->triggerClick();
        pump();
    };
    auto checkIn = [&](unsigned mask, bool editable)
    {
        for (int b = 0; b < 4; ++b)
        {
            auto* button = find<juce::TextButton>(*editor, bandId(b, "enabled"));
            expect(button && button->getToggleState() == ((mask & (1u << b)) != 0), "IN display is independent of SOLO");
            expect(button && button->isEnabled() == editable, "IN stays editable during SOLO");
            expect((p.state.getRawParameterValue(bandId(b, "enabled"))->load() > .5f) == (b == 1 || b == 2),
                   "SOLO never overwrites saved IN values");
        }
    };
    checkIn(6, true);
    clickSolo(3); checkIn(6, true);
    LinkRuntime runtime;
    auto snapshot = readSnapshot(p.state, runtime);
    expect(snapshot.bands[3].solo && !snapshot.bands[3].enabled, "SOLO never opens a muted input");
    clickSolo(1); checkIn(6, true);
    clickSolo(3); checkIn(6, true);
    clickSolo(1); checkIn(6, true);
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "solo"), 1.0f);
    pump(); checkIn(6, true);
    auto* in4 = find<juce::TextButton>(*editor, bandId(3, "enabled"));
    in4->triggerClick(); pump();
    expect(in4->getToggleState() && p.state.getRawParameterValue(bandId(3, "solo"))->load() > .5f,
           "IN can be toggled on while all SOLO buttons stay on");
    in4->triggerClick(); pump();
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "solo"), 0.0f);
    // Automation uses the same presentation and does not depend on button callbacks.
    set(p, bandId(3, "solo"), 1.0f); pump(); checkIn(6, true);
    set(p, bandCount, 0.0f); pump(); checkIn(6, true);
    set(p, bandCount, 2.0f); pump(); checkIn(6, true);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    editor.reset();
    editor.reset(p.createEditor()); host.addAndMakeVisible(*editor);
    expect(editor->getWidth() == 1330 && editor->getHeight() == 950, "editor reopen preserves chosen size");
    checkIn(6, true);
    set(p, bandId(3, "solo"), 0.0f); pump(); checkIn(6, true);

    PonteMC2000AudioProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    expect(restored.editorWidth.load() == 1330 && restored.editorHeight.load() == 950, "session restores editor size");
    set(restored, bandId(3, "solo"), 0.0f);
    for (int b = 0; b < 4; ++b)
        expect((restored.state.getRawParameterValue(bandId(b, "enabled"))->load() > .5f) == (b == 1 || b == 2),
               "saving in SOLO preserves baseline IN on reload");

    // Produce a real JUCE rendering with IN and SOLO simultaneously active.
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "solo"), 1.0f);
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "enabled"), 1.0f);
    pump();
    editor->setSize(1100, 738);
    const auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory().getChildFile("MC2000_UI_minimum.png");
    juce::FileOutputStream stream(file);
    if (stream.openedOk())
    {
        stream.setPosition(0); stream.truncate();
        juce::PNGImageFormat png;
        expect(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), stream), "UI snapshot renders");
    }
}

void testKnobEditing()
{
    using namespace pontedsp::mc2000::parameters;
    PonteMC2000AudioProcessor p;
    struct FixtureEditor : juce::AudioProcessorEditor
    {
        explicit FixtureEditor(juce::AudioProcessor& processor) : AudioProcessorEditor(processor) {}
    } fixture(p);
    fixture.setSize(300, 200);
    offscreenPeer(fixture);
    ParameterKnob first(p.state, inputGain, "INPUT", " dB", {}, 1);
    ParameterKnob second(p.state, outputGain, "OUTPUT", " dB", {}, 1);
    fixture.addAndMakeVisible(first); fixture.addAndMakeVisible(second);
    first.setBounds(20, 40, 100, 120); second.setBounds(160, 40, 100, 120);
    pontedsp::gui::ControlFocus focus(fixture);
    first.registerFocus(focus); second.registerFocus(focus);
    auto* firstValue = find<juce::Label>(fixture, juce::String(inputGain) + ".value");
    auto* secondValue = find<juce::Label>(fixture, juce::String(outputGain) + ".value");
    expect(firstValue && secondValue, "knob overlays are attached to editor");
    if (!firstValue || !secondValue) return;
    focus.pointerActivity(&first, true);
    expect(firstValue->isVisible() && !secondValue->isVisible(), "only active knob has a value overlay");
    expect(firstValue->getBottom() < first.getY(), "value overlay is above the knob");
    focus.pointerActivity(firstValue);
    expect(firstValue->isVisible(), "moving into value overlay retains knob focus");
    firstValue->showEditor();
    expect(firstValue->isBeingEdited(), "numeric editor opens");
    if (auto* text = firstValue->getCurrentTextEditor()) text->setText("-12.3 dB");
    focus.pointerActivity(&second);
    expect(!firstValue->isVisible() && secondValue->isVisible(), "handoff closes previous popup immediately");
    expect(std::abs(p.state.getRawParameterValue(inputGain)->load() + 12.3f) < 0.001f,
           "focus handoff commits edited value through parameter attachment");
    focus.pointerActivity(&first);
    firstValue->showEditor();
    if (auto* text = firstValue->getCurrentTextEditor()) text->setText("-5 dB");
    firstValue->hideEditor(true);
    expect(std::abs(p.state.getRawParameterValue(inputGain)->load() + 12.3f) < 0.001f,
           "cancelling numeric edit retains previous parameter value");
    auto* slider = find<juce::Slider>(first);
    expect(slider && !slider->isDoubleClickReturnEnabled(), "attachment cannot reinstate double-click reset");
    if (slider)
    {
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), { 20, 20 },
            {}, 1.0f, 0, 0, 0, 0, slider, slider, now, { 20, 20 }, now, 2, false);
        slider->mouseDoubleClick(event);
        first.mouseDoubleClick(event);
        expect(firstValue->isBeingEdited(), "double-click on knob opens numeric editor");
        expect(std::abs(p.state.getRawParameterValue(inputGain)->load() + 12.3f) < .001f,
               "double-click edits without resetting the parameter first");
        if (auto* text = firstValue->getCurrentTextEditor()) text->setText("-8.4 dB");
        firstValue->hideEditor(false);
        expect(std::abs(p.state.getRawParameterValue(inputGain)->load() + 8.4f) < .001f,
               "knob double-click edit commits through automation attachment");
    }

}

void testSoloAudioRouting()
{
    using namespace pontedsp::mc2000;
    for (const auto enabledMask : { 0u, 6u, 15u })
    for (const auto mask : { 8u, 10u, 15u, 0u })
    {
        const auto audibleMask = enabledMask & (mask == 0 ? 15u : mask);
        PonteMC2000AudioProcessor p;
        for (int b = 0; b < 4; ++b)
        {
            set(p, parameters::bandId(b, "enabled"), (enabledMask & (1u << b)) != 0 ? 1.0f : 0.0f);
            set(p, parameters::bandId(b, "solo"), (mask & (1u << b)) != 0 ? 1.0f : 0.0f);
            set(p, parameters::bandId(b, "ratio"), 1.0f);
        }
        p.prepareToPlay(48000.0, 256);
        dsp::CrossoverNetwork reference;
        reference.prepare(48000.0, 1);
        reference.setBandCount(4);
        reference.setFrequencies({ 100.0, 1000.0, 10000.0 });
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        double maxError {}, energy {};
        for (int block = 0; block < 64; ++block)
        {
            std::array<double, 256> expected {};
            for (int n = 0; n < 256; ++n)
            {
                const auto t = (block * 256 + n) / 48000.0;
                const auto sample = static_cast<float>(.1 * (std::sin(juce::MathConstants<double>::twoPi * 60 * t)
                    + std::sin(juce::MathConstants<double>::twoPi * 500 * t)
                    + std::sin(juce::MathConstants<double>::twoPi * 12000 * t)));
                audio.setSample(0, n, sample); audio.setSample(1, n, sample);
                std::array<double, 4> split {};
                reference.processSample(0, sample, split);
                for (unsigned b = 0; b < 4; ++b)
                    if ((audibleMask & (1u << b)) != 0) expected[static_cast<size_t>(n)] += split[b];
            }
            p.processBlock(audio, midi);
            // SOLO uses a 5 ms exponential time constant, not a finite 5 ms ramp.
            // Compare the settled sum after >20 time constants (100 ms).
            if (block >= 20)
                for (int n = 0; n < 256; ++n)
                {
                    const auto value = expected[static_cast<size_t>(n)];
                    maxError = std::max(maxError, std::abs(audio.getSample(0, n) - value));
                    energy += value * value;
                }
        }
        const auto expectedEnergy = audibleMask == 0 ? energy == 0 : energy > .01;
        if (!(expectedEnergy && maxError < 2.0e-6))
            std::cerr << "IN/SOLO masks=" << enabledMask << "/" << mask << " error=" << maxError << '\n';
        expect(expectedEnergy && maxError < 2.0e-6,
               "audio contains only IN bands allowed by SOLO, including silence and all bands together");
    }
}
void testHeaderVersions()
{
    using namespace pontedsp::gui;
    expect(isNewerRelease("v0.2.10", "0.2.9"), "versions compare numerically");
    expect(!isNewerRelease("0.2.2", "0.2.3") && !isNewerRelease("0.2.3", "0.2.3"), "no notification for old or same version");
    expect(!isNewerRelease("0.2.4-beta", "0.2.3") && !parseReleaseVersion("1..3")
           && !parseReleaseVersion("9999999.0.0"), "malformed and prerelease tags are rejected");
    expect(versionFromReleaseJson(R"({"tag_name":"v0.2.4","draft":false,"prerelease":false})") == "0.2.4", "stable release JSON accepted");
    expect(versionFromReleaseJson(R"({"tag_name":"v0.2.4","draft":true,"prerelease":false})").isEmpty()
           && versionFromReleaseJson(R"({"tag_name":"v0.2.4","draft":false,"prerelease":true})").isEmpty()
           && versionFromReleaseJson(R"({"message":"API rate limit exceeded"})").isEmpty()
           && versionFromReleaseJson("offline").isEmpty(), "drafts, prereleases and network errors cannot advertise an update");
    ContextHeader header;
    header.setSize(168, 54);
    expect(header.versionText() == JucePlugin_VersionString, "header uses build version");
    auto parts = *parseReleaseVersion(JucePlugin_VersionString);
    const auto nextVersion = juce::String(parts[0]) + "." + juce::String(parts[1]) + "." + juce::String(parts[2] + 1);
    const auto laterVersion = juce::String(parts[0]) + "." + juce::String(parts[1]) + "." + juce::String(parts[2] + 2);
    header.setLatestVersion(nextVersion);
    expect(header.versionText().contains(nextVersion), "header shows newer version");
    header.setHelpText("Help replaces the complete header.");
    const auto before = header.createComponentSnapshot(header.getLocalBounds());
    header.setLatestVersion(laterVersion);
    const auto after = header.createComponentSnapshot(header.getLocalBounds());
    bool equal = true;
    for (int y = 0; y < before.getHeight(); ++y)
        for (int x = 0; x < before.getWidth(); ++x)
            equal = equal && before.getPixelAt(x, y) == after.getPixelAt(x, y);
    expect(equal && header.isShowingHelp(), "help completely hides changing version notification");
    header.setHelpText({});
    expect(header.versionText().contains(laterVersion) && !header.isShowingHelp(), "closing help restores current notification");
}

void testClosedSpectrumAudio()
{
    PonteMC2000AudioProcessor closed, opened;
    closed.prepareToPlay(48000, 512); opened.prepareToPlay(48000, 512);
    std::array<PonteMC2000AudioProcessor::SpectrumSample, 512> samples;
    juce::MidiBuffer midi;
    auto plot = std::make_unique<CrossoverPlot>(opened);
    for (int block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> a(2,512), b(2,512);
        for (int i = 0; i < 512; ++i)
            for (int ch = 0; ch < 2; ++ch)
                a.setSample(ch,i,static_cast<float>(.4*std::sin((block*512+i)*.04)));
        b.makeCopyOf(a);
        closed.processBlock(a,midi); opened.processBlock(b,midi);
        bool identical = true;
        for (int ch=0; ch<2; ++ch) for (int i=0; i<512; ++i)
            identical = identical && a.getSample(ch,i)==b.getSample(ch,i);
        expect(identical, "opening analyzer does not change a single audio sample");
        bool gap = false;
        expect(closed.popSpectrumSamples(samples.data(),512,gap)==0, "closed GUI produces no spectrum traffic");
        expect(opened.popSpectrumSamples(samples.data(),512,gap)==512, "open analyzer captures current samples");
    }
    plot.reset();
    juce::AudioBuffer<float> silence(2,512); silence.clear();
    opened.processBlock(silence,midi);
    bool gap = false;
    expect(opened.popSpectrumSamples(samples.data(),512,gap)==0, "destroying last analyzer stops FIFO production");
}

void testCachedParameterRestore()
{
    using namespace pontedsp::mc2000::parameters;
    PonteMC2000AudioProcessor p;
    p.prepareToPlay(48000, 64);
    set(p,inputGain,3.0f);
    set(p,bandId(1,"ratio"),4.0f);
    set(p,bandId(1,"tcMode"),2.0f);
    juce::MemoryBlock saved; p.getStateInformation(saved);
    const auto process = [&]
    {
        juce::AudioBuffer<float> audio(2,64); audio.clear(); juce::MidiBuffer midi;
        p.processBlock(audio,midi);
    };
    set(p,inputGain,-6.0f); set(p,bandId(1,"ratio"),8.0f);
    process();
    expect(p.getEngine().getParameters().inputGainDb == -6
           && p.getEngine().getParameters().bands[1].ratio == 8, "cached parameter handles follow automation");
    p.setStateInformation(saved.getData(),static_cast<int>(saved.getSize())); process();
    expect(p.getEngine().getParameters().inputGainDb == 3
           && p.getEngine().getParameters().bands[1].ratio == 4
           && p.getEngine().getParameters().bands[1].tcMode == pontedsp::mc2000::dsp::TCMode::automatic,
           "cached parameter handles survive state replacement");
    set(p,linkMaster,2.0f); process();
    set(p,bandId(1,"ratio"),5.0f); process();
    expect(p.getEngine().getParameters().bands[0].ratio == 2
           && p.getEngine().getParameters().bands[1].ratio == 5,
           "cached snapshot preserves relative linked control offsets");
}

void testSpectrumTimingAndTails()
{
    using namespace pontedsp::mc2000::parameters;
    PonteMC2000AudioProcessor p;
    p.prepareToPlay(48000, 512);
    CrossoverPlot plot(p);
    plot.setSize(570, 200);
    juce::MidiBuffer midi;
    const auto sendTone = [&](int samples)
    {
        juce::AudioBuffer<float> audio(2, samples);
        for (int i = 0; i < samples; ++i)
        {
            const auto value = static_cast<float>(.5 * std::sin(2 * juce::MathConstants<double>::pi * 375 * i / 48000));
            audio.setSample(0, i, value); audio.setSample(1, i, -value);
        }
        p.processBlock(audio, midi);
    };
    sendTone(2048);
    plot.updateSpectrum(2048.0 / 48000);
    const auto peak = plot.displayedSpectrumDb(16);
    expect(std::abs(peak + 6.0206f) < .03f,
           "first full FFT reaches input peak immediately, including anti-phase stereo");
    const auto image = plot.createComponentSnapshot(plot.getLocalBounds());
    expect(image.isValid() && plot.displayedSpectrumDb(16) == peak, "FFT repaint is passive");
    pontedsp::gui::LevelMeterBallistics reference;
    reference.update(peak, 0);
    plot.updateSpectrum(.25);
    expect(std::abs(plot.displayedSpectrumDb(16) - reference.update(-100, .25)) < .001,
           "spectrum and level meter share the same elapsed-time fall");
    for (int b = 0; b < 4; ++b) set(p, bandId(b, "enabled"), b == 1 ? 1.0f : 0.0f);
    plot.updateSpectrum(.033);
    const auto below = plot.inputResponseDb(50);
    expect(below > -26 && below < -24, "band 2 retains its LR4 tail below the 100 Hz crossover");
    expect(std::abs(plot.inputResponseDb(99.9) - plot.inputResponseDb(100.1)) < .05,
           "spectrum response is continuous across crossover markers");
    set(p, bandId(3, "solo"), 1.0f);
    expect(plot.inputResponseDb(50) == below, "output SOLO does not crop the input spectrum");
    plot.updateSpectrum(12);
    expect(plot.displayedSpectrumDb(16) < -99, "spectrum empties after audio callbacks stop");

    // Empty GUI reads between normal, small host blocks must not prevent
    // the first FFT window from ever completing at a low sample rate.
    p.prepareToPlay(8000, 512);
    auto slowRate = std::make_unique<CrossoverPlot>(p);
    for (int block = 0; block < 4; ++block)
    {
        sendTone(512);
        slowRate->updateSpectrum(.033);
        if (block < 3) slowRate->updateSpectrum(.031);
    }
    expect(slowRate->displayedSpectrumDb(16) > -60, "partial FFT survives normal gaps between host blocks");
    p.prepareToPlay(48000, 512);
    // FIFO overload must not replay a backlog; normal truncation keeps the latest samples.
    for (int block = 0; block < 9; ++block) sendTone(4096);
    std::array<PonteMC2000AudioProcessor::SpectrumSample, 32> recent;
    bool gap = false;
    expect(p.popSpectrumSamples(recent.data(), 32, gap) == 0 && gap,
           "overflow discards stale audio instead of drawing it late");
    juce::AudioBuffer<float> fresh(2, 512);
    for (int i = 0; i < 512; ++i)
    {
        fresh.setSample(0, i, static_cast<float>(i) / 1024);
        fresh.setSample(1, i, -static_cast<float>(i) / 1024);
    }
    p.processBlock(fresh, midi);
    const auto count = p.popSpectrumSamples(recent.data(), 32, gap);
    expect(count == 32 && gap && recent.front()[0] == 480.0f / 1024
           && recent.back()[1] == -511.0f / 1024,
           "bounded spectrum read returns newest stereo samples, not oldest samples");
    sendTone(512);
    CrossoverPlot reopened(p);
    expect(p.popSpectrumSamples(recent.data(), 32, gap) == 0,
           "opening a spectrum discards the closed-editor history");
}

void testDotMeterCoherence()
{
    PonteMC2000AudioProcessor p;
    p.prepareToPlay(48000, 512);
    PonteMC2000AudioProcessorEditor editor(p);
    auto* band = find<BandMeter>(editor);
    auto* plot = find<CompressionPlot>(editor);
    juce::AudioBuffer<float> audio(2, 512);
    for (int i = 0; i < 512; ++i)
    {
        const auto value = static_cast<float>(.5 * std::sin(2 * juce::MathConstants<double>::pi * 50 * i / 48000));
        audio.setSample(0, i, value); audio.setSample(1, i, value);
    }
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    pump();
    expect(band && plot && plot->displayedInputDb(0) == band->displayedValues().inputDb
           && plot->displayedInputDb(0) > -30,
           "STATIC I/O dot uses the same captured and smoothed input as its band meter");
    pump();
    expect(plot->displayedInputDb(0) == band->displayedValues().inputDb,
           "dot and meter decay remain synchronized without callbacks");
}

void testMeterPaintingAndReopen()
{
    PonteMC2000AudioProcessor processor;
    auto& engine = processor.getEngine();
    engine.prepare(48000, 512, 2);
    auto sendTone = [&]
    {
        std::vector<float> left(512), right(512);
        for (int i = 0; i < 512; ++i)
            left[i] = right[i] = static_cast<float>(.5 * std::sin(2 * 3.141592653589793 * 315 * i / 48000));
        float* audio[] { left.data(), right.data() };
        engine.process(audio, 2, 512);
    };
    {
        BandMeter band(processor, 1);
        OutputMeter main(processor);
        band.setSize(360, 60);
        main.setSize(360, 40);
        sendTone();
        // A repaint before the timer must not consume or display new peaks.
        band.createComponentSnapshot(band.getLocalBounds());
        main.createComponentSnapshot(main.getLocalBounds());
        expect(band.displayedValues().inputDb == -100 && main.displayedValues()[0] == -100,
               "painting does not read or consume pending audio peaks");
        band.update(1.0 / 30);
        main.update(1.0 / 30);
        const auto bandBefore = band.displayedValues().inputDb;
        const auto mainBefore = main.displayedValues()[0];
        expect(bandBefore > -12 && mainBefore > -12, "GUI tick captures peaks after an early repaint");
        for (int i = 0; i < 5; ++i)
        {
            band.createComponentSnapshot(band.getLocalBounds());
            main.createComponentSnapshot(main.getLocalBounds());
        }
        expect(band.displayedValues().inputDb == bandBefore && main.displayedValues()[0] == mainBefore,
               "extra paints cannot accelerate meter decay");
        band.update(12);
        main.update(12);
        expect(band.displayedValues().inputDb < -99 && main.displayedValues()[0] < -99,
               "meters empty when the DAW suspends audio callbacks");
    }
    sendTone();
    {
        PonteMC2000AudioProcessorEditor editor(processor);
        expect(engine.consumeOutputMeterDb()[0] == -100
               && engine.consumeBandMeter(1).inputDb == -100,
               "opening an editor discards peaks accumulated while closed");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    testTiming();
    testHeaderVersions();
    testClosedSpectrumAudio();
    testCachedParameterRestore();
    testFocusController();
    testEditorAndSolo();
    testKnobEditing();
    testSoloAudioRouting();
    testMeterPaintingAndReopen();
    testSpectrumTimingAndTails();
    testDotMeterCoherence();
    if (failures == 0) std::cout << "All Ponte MC2000 UI/state tests passed\n";
    return failures == 0 ? 0 : 1;
}

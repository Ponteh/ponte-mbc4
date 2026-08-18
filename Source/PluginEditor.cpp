#include "PluginEditor.h"
#include "Parameters.h"
#include "PontePalette.h"
#include <cmath>

namespace {

const std::array<juce::Colour, 4> bandColours {
    juce::Colour(0xffffd166), juce::Colour(0xff5bd18b),
    juce::Colour(0xffff8c42), juce::Colour(0xffb57cff)
};

void configureLabel(juce::Label& label, const juce::String& text, const float size,
                    const juce::Justification justification = juce::Justification::centred)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(size, juce::Font::bold));
    label.setJustificationType(justification);
    label.setColour(juce::Label::textColourId, pontedsp::gui::Palette::text());
}

void setContextHelp(juce::Component& component, const juce::String& text)
{
    component.getProperties().set("mbc4ContextHelp", text);
}

float meterPosition(const float db) noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

} // namespace

ParameterKnob::ParameterKnob(juce::AudioProcessorValueTreeState& state,
                             const juce::String& parameterId,
                             const juce::String& caption, const juce::String& suffix,
                             const juce::String& helpText)
{
    setContextHelp(*this, helpText);
    configureLabel(name, caption, 10.0f);
    addAndMakeVisible(name);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    slider.setDoubleClickReturnValue(true,
        state.getParameterRange(parameterId).convertFrom0to1(
            state.getParameter(parameterId)->getDefaultValue()));
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setMouseDragSensitivity(180);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, parameterId, slider);
}

void ParameterKnob::resized()
{
    auto bounds = getLocalBounds();
    name.setBounds(bounds.removeFromTop(15));
    slider.setBounds(bounds);
}

void ContextHeader::setHelpText(const juce::String& text)
{
    if (helpText == text) return;
    helpText = text;
    repaint();
}

void ContextHeader::setBandCount(const int count)
{
    if (bandCount == count) return;
    bandCount = count;
    repaint();
}

void ContextHeader::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().reduced(3, 2);
    if (helpText.isNotEmpty())
    {
        g.setColour(pontedsp::gui::Palette::text());
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawFittedText(helpText, area, juce::Justification::centredLeft, 3, 0.9f);
        return;
    }

    auto title = area;
    auto subtitle = title.removeFromBottom(20);
    g.setColour(pontedsp::gui::Palette::text());
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawFittedText("PONTE DSP", title, juce::Justification::centredLeft, 1);
    g.setFont(juce::FontOptions(18.0f));
    g.drawFittedText("MBC4", subtitle, juce::Justification::centredLeft, 1);
}

CrossoverField::CrossoverField(juce::AudioProcessorValueTreeState& valueTreeState,
                               const int crossoverIndex)
    : state(valueTreeState), index(crossoverIndex)
{
    value.setEditable(true, true, false);
    value.setJustificationType(juce::Justification::centred);
    value.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    value.setColour(juce::Label::textColourId, pontedsp::gui::Palette::text());
    value.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    value.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    value.setColour(juce::Label::textWhenEditingColourId, pontedsp::gui::Palette::text());
    value.addListener(this);
    addAndMakeVisible(value);
    setContextHelp(*this, "Type crossover X" + juce::String(index + 1)
        + " in Hz, or drag its marker. Crossovers cannot pass each other.");
    timerCallback();
    startTimerHz(15);
}

CrossoverField::~CrossoverField()
{
    value.removeListener(this);
}

double CrossoverField::valueFromText(const juce::String& input)
{
    auto text = input.trim().toLowerCase().removeCharacters("hz ");
    auto multiplier = 1.0;
    if (text.endsWithChar('k'))
    {
        multiplier = 1000.0;
        text = text.dropLastCharacters(1);
    }
    return text.getDoubleValue() * multiplier;
}

juce::String CrossoverField::textFromValue(const double frequency)
{
    return juce::String(juce::roundToInt(frequency));
}

double CrossoverField::constrainedFrequency(const double requested) const noexcept
{
    auto frequency = juce::jlimit(20.0, 20000.0, requested);
    const auto count = juce::jlimit(2, 4, static_cast<int>(
        state.getRawParameterValue(pontedsp::mc2000::parameters::bandCount)->load()) + 2);
    if (index > 0)
        frequency = std::max(frequency, static_cast<double>(state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(index - 1))->load()) * 1.01);
    if (index < count - 2)
        frequency = std::min(frequency, static_cast<double>(state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(index + 1))->load()) / 1.01);
    return juce::jlimit(20.0, 20000.0, frequency);
}

void CrossoverField::labelTextChanged(juce::Label*)
{
    if (updating) return;
    const auto frequency = constrainedFrequency(valueFromText(value.getText()));
    if (auto* parameter = state.getParameter(pontedsp::mc2000::parameters::crossoverId(index)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(frequency)));
        parameter->endChangeGesture();
    }
    updating = true;
    value.setText(textFromValue(frequency), juce::dontSendNotification);
    updating = false;
}

void CrossoverField::editorShown(juce::Label*, juce::TextEditor& editor)
{
    editor.selectAll();
}

void CrossoverField::timerCallback()
{
    if (value.getCurrentTextEditor() != nullptr) return;
    const auto frequency = state.getRawParameterValue(
        pontedsp::mc2000::parameters::crossoverId(index))->load();
    const auto text = textFromValue(frequency);
    if (value.getText() != text)
    {
        updating = true;
        value.setText(text, juce::dontSendNotification);
        updating = false;
    }
}

void CrossoverField::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(pontedsp::gui::Palette::ink().withAlpha(0.88f));
    g.fillRoundedRectangle(area, 7.0f);
    g.setColour(hasKeyboardFocus(true) ? pontedsp::gui::Palette::lime()
                                       : pontedsp::gui::Palette::outline());
    g.drawRoundedRectangle(area, 7.0f, 1.2f);
}

void CrossoverField::resized()
{
    value.setBounds(getLocalBounds().reduced(4, 1));
}

BandMeter::BandMeter(PonteMC2000AudioProcessor& p, const int bandIndex)
    : processor(p), band(bandIndex)
{
    setInterceptsMouseClicks(false, false);
}

void BandMeter::paint(juce::Graphics& g)
{
    const auto snapshot = processor.getEngine().getBandMeter(band);
    auto area = getLocalBounds().toFloat().reduced(3.0f);
    const auto row = area.getHeight() / 3.0f;
    const std::array<float, 3> positions {
        meterPosition(snapshot.inputDb), meterPosition(snapshot.outputDb),
        juce::jlimit(0.0f, 1.0f, snapshot.gainReductionDb / 30.0f)
    };
    const std::array<const char*, 3> labels { "IN", "OUT", "GR" };
    for (int i = 0; i < 3; ++i)
    {
        auto line = area.removeFromTop(row).reduced(0.0f, 3.0f);
        g.setColour(pontedsp::gui::Palette::mutedText());
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(labels[static_cast<std::size_t>(i)], line.removeFromLeft(24.0f),
                   juce::Justification::centredLeft);
        g.setColour(pontedsp::gui::Palette::ink());
        g.fillRoundedRectangle(line, 2.0f);
        g.setColour(i == 2 ? pontedsp::gui::Palette::danger()
                           : bandColours[static_cast<std::size_t>(band)]);
        g.fillRoundedRectangle(line.withWidth(line.getWidth()
            * positions[static_cast<std::size_t>(i)]), 2.0f);
    }
}

BandStrip::BandStrip(PonteMC2000AudioProcessor& p, const int bandIndex)
    : processor(p), band(bandIndex),
      gain(p.state, pontedsp::mc2000::parameters::bandId(band, "gainDb"), "GAIN", " dB",
           "Add post-compression makeup gain to this band."),
      threshold(p.state, pontedsp::mc2000::parameters::bandId(band, "thresholdDb"), "THRESH", " dB",
                "Set the peak level where compression starts for this band."),
      ratio(p.state, pontedsp::mc2000::parameters::bandId(band, "ratio"), "RATIO", " :1",
            "Set how strongly signals above the threshold are compressed."),
      knee(p.state, pontedsp::mc2000::parameters::bandId(band, "knee"), "KNEE", {},
           "Shape the transition around threshold: undershoot, hard knee, overshoot or tail."),
      bite(p.state, pontedsp::mc2000::parameters::bandId(band, "bite"), "BITE", {},
           "Let more transient detail pass while preserving steady-state compression."),
      attack(p.state, pontedsp::mc2000::parameters::bandId(band, "attackMs"), "ATTACK", " ms",
             "Set how quickly gain reduction reacts to a rising signal."),
      release(p.state, pontedsp::mc2000::parameters::bandId(band, "releaseMs"), "RELEASE", " ms",
              "Set how quickly gain reduction returns after the signal falls."),
      meter(p, band)
{
    configureLabel(title, "BAND " + juce::String(band + 1), 13.0f,
                   juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, bandColours[static_cast<std::size_t>(band)]);
    addAndMakeVisible(title);
    for (auto* component : std::array<juce::Component*, 11> {
        &enabled, &solo, &gain, &threshold, &ratio, &knee, &bite, &attack, &release,
        &timeConstant, &meter })
        addAndMakeVisible(*component);
    enabled.setClickingTogglesState(true);
    solo.setClickingTogglesState(true);
    timeConstant.addItemList({ "R1", "R2", "AUTO" }, 1);
    setContextHelp(enabled, "Enable or bypass compression for this band. Makeup gain remains active.");
    setContextHelp(solo, "Monitor this band after crossover and compression. Multiple bands may be soloed.");
    setContextHelp(timeConstant, "Choose Pure Peak R1, adaptive release R2, or program-dependent Auto timing.");
    setContextHelp(meter, "Monitor band input, output and gain reduction levels.");
    enabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, pontedsp::mc2000::parameters::bandId(band, "enabled"), enabled);
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, pontedsp::mc2000::parameters::bandId(band, "solo"), solo);
    timeConstantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, pontedsp::mc2000::parameters::bandId(band, "tcMode"), timeConstant);
}

void BandStrip::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(pontedsp::gui::Palette::surface().withAlpha(0.94f));
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(bandColours[static_cast<std::size_t>(band)].withAlpha(0.42f));
    g.drawRoundedRectangle(area, 8.0f, 1.0f);
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced(8, 5);
    auto identity = area.removeFromLeft(96);
    title.setBounds(identity.removeFromTop(24));
    enabled.setBounds(identity.removeFromTop(30).reduced(2));
    solo.setBounds(identity.removeFromTop(30).reduced(2));
    auto meterArea = area.removeFromRight(142);
    timeConstant.setBounds(meterArea.removeFromTop(30).reduced(4, 1));
    meter.setBounds(meterArea);
    const auto knobWidth = area.getWidth() / 7;
    for (auto* knob : std::array<ParameterKnob*, 7> {
        &gain, &threshold, &ratio, &knee, &bite, &attack, &release })
        knob->setBounds(area.removeFromLeft(knobWidth));
}

CrossoverPlot::CrossoverPlot(PonteMC2000AudioProcessor& p) : processor(p)
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

int CrossoverPlot::currentBandCount() const noexcept
{
    return juce::jlimit(2, 4, static_cast<int>(processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandCount)->load()) + 2);
}

float CrossoverPlot::frequencyToX(const double frequency) const noexcept
{
    return static_cast<float>(std::log(frequency / 20.0) / std::log(1000.0))
         * static_cast<float>(getWidth());
}

double CrossoverPlot::xToFrequency(const float x) const noexcept
{
    return 20.0 * std::pow(1000.0, juce::jlimit(0.0, 1.0,
        static_cast<double>(x) / std::max(1, getWidth())));
}

void CrossoverPlot::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour(pontedsp::gui::Palette::ink().withAlpha(0.92f));
    g.fillRoundedRectangle(area, 8.0f);
    g.setFont(juce::FontOptions(9.0f));
    for (const auto frequency : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
    {
        const auto x = frequencyToX(frequency);
        g.setColour(pontedsp::gui::Palette::outline().withAlpha(0.5f));
        g.drawVerticalLine(juce::roundToInt(x), 16.0f, area.getBottom() - 5.0f);
        g.setColour(pontedsp::gui::Palette::mutedText());
        const auto label = frequency >= 1000.0
            ? juce::String(frequency / 1000.0, 0) + "k"
            : juce::String(static_cast<int>(frequency));
        g.drawText(label, juce::roundToInt(x) - 18, 1, 36, 14, juce::Justification::centred);
    }
    for (int band = 0; band < currentBandCount(); ++band)
    {
        juce::Path path;
        for (int point = 0; point <= 180; ++point)
        {
            const auto x = area.getWidth() * static_cast<float>(point) / 180.0f;
            const auto db = processor.getEngine().getBandMagnitudeDb(band, xToFrequency(x));
            const auto y = juce::jmap(static_cast<float>(juce::jlimit(-48.0, 3.0, db)),
                                     3.0f, -48.0f, 18.0f, area.getBottom() - 8.0f);
            if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(bandColours[static_cast<std::size_t>(band)]);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }
    for (int crossover = 0; crossover < currentBandCount() - 1; ++crossover)
    {
        const auto frequency = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(crossover))->load();
        const auto x = frequencyToX(frequency);
        g.setColour(pontedsp::gui::Palette::text());
        g.fillEllipse(x - 4.0f, area.getCentreY() - 4.0f, 8.0f, 8.0f);
    }
}

void CrossoverPlot::mouseDown(const juce::MouseEvent& event)
{
    auto closest = 100000.0f;
    for (int crossover = 0; crossover < currentBandCount() - 1; ++crossover)
    {
        const auto x = frequencyToX(processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(crossover))->load());
        if (const auto distance = std::abs(event.position.x - x); distance < closest)
        {
            closest = distance;
            draggedCrossover = crossover;
        }
    }
    if (draggedCrossover >= 0)
        processor.state.getParameter(
            pontedsp::mc2000::parameters::crossoverId(draggedCrossover))->beginChangeGesture();
}

void CrossoverPlot::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedCrossover < 0) return;
    auto frequency = xToFrequency(event.position.x);
    if (draggedCrossover > 0)
        frequency = std::max(frequency, static_cast<double>(processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(draggedCrossover - 1))->load()) * 1.01);
    if (draggedCrossover < currentBandCount() - 2)
        frequency = std::min(frequency, static_cast<double>(processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(draggedCrossover + 1))->load()) / 1.01);
    if (auto* parameter = processor.state.getParameter(
        pontedsp::mc2000::parameters::crossoverId(draggedCrossover)))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(frequency)));
    repaint();
}

void CrossoverPlot::mouseUp(const juce::MouseEvent&)
{
    if (draggedCrossover >= 0)
        processor.state.getParameter(
            pontedsp::mc2000::parameters::crossoverId(draggedCrossover))->endChangeGesture();
    draggedCrossover = -1;
}

void CompressionPlot::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour(pontedsp::gui::Palette::ink().withAlpha(0.92f));
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(pontedsp::gui::Palette::outline());
    g.drawLine(0.0f, area.getBottom(), area.getRight(), 0.0f, 1.0f);
    const auto count = juce::jlimit(2, 4, static_cast<int>(processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandCount)->load()) + 2);
    for (int band = 0; band < count; ++band)
    {
        juce::Path path;
        for (int point = 0; point <= 120; ++point)
        {
            const auto input = -60.0 + 60.0 * static_cast<double>(point) / 120.0;
            const auto output = processor.getEngine().getStaticOutputDb(band, input);
            const auto x = area.getWidth() * static_cast<float>(point) / 120.0f;
            const auto y = juce::jmap(static_cast<float>(output), -60.0f, 0.0f,
                                     area.getBottom(), 0.0f);
            if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(bandColours[static_cast<std::size_t>(band)]);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }
    g.setColour(pontedsp::gui::Palette::mutedText());
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("STATIC I/O", 8, 5, 80, 14, juce::Justification::centredLeft);
}

void OutputMeter::paint(juce::Graphics& g)
{
    const auto levels = processor.getEngine().getOutputMeterDb();
    auto area = getLocalBounds().toFloat().reduced(3.0f);
    for (int channel = 0; channel < 2; ++channel)
    {
        auto row = area.removeFromTop(area.getHeight()
            / static_cast<float>(2 - channel)).reduced(0.0f, 3.0f);
        g.setColour(pontedsp::gui::Palette::ink());
        g.fillRoundedRectangle(row, 2.0f);
        g.setColour(levels[static_cast<std::size_t>(channel)] > -0.1f
                        ? pontedsp::gui::Palette::danger() : pontedsp::gui::Palette::lime());
        g.fillRoundedRectangle(row.withWidth(row.getWidth()
            * meterPosition(levels[static_cast<std::size_t>(channel)])), 2.0f);
    }
}

PonteMC2000AudioProcessorEditor::PonteMC2000AudioProcessorEditor(PonteMC2000AudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      inputGain(p.state, pontedsp::mc2000::parameters::inputGain, "INPUT", " dB",
                "Adjust the level feeding the crossover and all compressor bands."),
      outputGain(p.state, pontedsp::mc2000::parameters::outputGain, "OUTPUT", " dB",
                 "Adjust the final level after all processed bands are summed."),
      crossoverPlot(p), compressionPlot(p), outputMeter(p)
{
    setLookAndFeel(&lookAndFeel);
    configureLabel(crossoverLabel, "CROSSOVER", 10.0f);
    configureLabel(bandCountLabel, "MODE", 9.0f);
    configureLabel(linkLabel, "LINK", 9.0f);
    bandCount.addItemList({ "2 BAND", "3 BAND", "4 BAND" }, 1);
    linkMaster.addItemList({ "UNLINKED", "MASTER 1", "MASTER 2", "MASTER 3", "MASTER 4" }, 1);
    phase.setClickingTogglesState(true);
    setContextHelp(phase, "Invert the polarity of the final output.");
    setContextHelp(bandCount, "Choose a two, three or four-band crossover layout.");
    setContextHelp(linkMaster, "Link band controls relatively to the selected master band.");
    setContextHelp(crossoverPlot, "View the LR4 band responses and drag crossover markers horizontally.");
    setContextHelp(compressionPlot, "View the static input-to-output transfer curve for every active band.");
    setContextHelp(outputMeter, "Monitor final left and right output peak levels.");
    for (auto* component : std::array<juce::Component*, 12> {
        &contextHeader, &inputGain, &outputGain, &phase, &bandCount, &linkMaster,
        &crossoverLabel, &bandCountLabel, &linkLabel, &crossoverPlot, &compressionPlot, &outputMeter })
        addAndMakeVisible(*component);
    for (int crossover = 0; crossover < 3; ++crossover)
    {
        crossoverFields[static_cast<std::size_t>(crossover)] =
            std::make_unique<CrossoverField>(p.state, crossover);
        addAndMakeVisible(*crossoverFields[static_cast<std::size_t>(crossover)]);
    }
    for (int band = 0; band < 4; ++band)
    {
        bands[static_cast<std::size_t>(band)] = std::make_unique<BandStrip>(p, band);
        addAndMakeVisible(*bands[static_cast<std::size_t>(band)]);
    }
    phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, pontedsp::mc2000::parameters::phaseInvert, phase);
    bandCountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, pontedsp::mc2000::parameters::bandCount, bandCount);
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, pontedsp::mc2000::parameters::linkMaster, linkMaster);
    displayedBandCount = activeBandCount();
    contextHeader.setBandCount(displayedBandCount);
    setResizable(true, true);
    setResizeLimits(1100, 580, 1600, 1100);
    setSize(1250, 298 + 146 * displayedBandCount);
    startTimerHz(30);
}

PonteMC2000AudioProcessorEditor::~PonteMC2000AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

int PonteMC2000AudioProcessorEditor::activeBandCount() const noexcept
{
    return juce::jlimit(2, 4, static_cast<int>(processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandCount)->load()) + 2);
}

void PonteMC2000AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(pontedsp::gui::Palette::ink());
    juce::ColourGradient gradient(pontedsp::gui::Palette::violet().withAlpha(0.42f),
                                  0.0f, 0.0f, pontedsp::gui::Palette::ink(),
                                  0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillRect(getLocalBounds());
}

void PonteMC2000AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(54);
    outputMeter.setBounds(header.removeFromRight(140).reduced(4, 8));
    contextHeader.setBounds(header.removeFromLeft(176));
    header.removeFromLeft(10);
    crossoverLabel.setBounds(header.removeFromLeft(72));
    const auto activeFields = activeBandCount() - 1;
    for (int crossover = 0; crossover < 3; ++crossover)
    {
        const auto visible = crossover < activeFields;
        crossoverFields[static_cast<std::size_t>(crossover)]->setVisible(visible);
        if (visible)
        {
            crossoverFields[static_cast<std::size_t>(crossover)]->setBounds(
                header.removeFromLeft(100).reduced(0, 9));
            header.removeFromLeft(10);
        }
    }
    header.removeFromLeft(8);
    bandCountLabel.setBounds(header.removeFromLeft(42));
    bandCount.setBounds(header.removeFromLeft(102).reduced(0, 9));
    header.removeFromLeft(10);
    linkLabel.setBounds(header.removeFromLeft(36));
    linkMaster.setBounds(header.removeFromLeft(122).reduced(0, 9));

    auto displays = area.removeFromTop(212);
    auto master = displays.removeFromLeft(180).reduced(3);
    phase.setBounds(master.removeFromBottom(32).reduced(8, 2));
    inputGain.setBounds(master.removeFromLeft(master.getWidth() / 2));
    outputGain.setBounds(master);
    auto compression = displays.removeFromRight(displays.getWidth() / 2).reduced(5);
    compressionPlot.setBounds(compression);
    crossoverPlot.setBounds(displays.reduced(5));

    area.removeFromTop(8);
    const auto count = activeBandCount();
    const auto stripHeight = area.getHeight() / count;
    for (int band = 0; band < 4; ++band)
    {
        const auto visible = band < count;
        bands[static_cast<std::size_t>(band)]->setVisible(visible);
        if (visible)
            bands[static_cast<std::size_t>(band)]->setBounds(
                area.removeFromTop(stripHeight).reduced(0, 3));
    }
}

void PonteMC2000AudioProcessorEditor::updateBandCountLayout()
{
    const auto count = activeBandCount();
    if (count == displayedBandCount) return;
    constexpr int fixedHeight = 298;
    const auto stripHeight = juce::jlimit(110, 180,
        (getHeight() - fixedHeight) / std::max(1, displayedBandCount));
    displayedBandCount = count;
    contextHeader.setBandCount(count);
    setSize(getWidth(), fixedHeight + stripHeight * count);
}

void PonteMC2000AudioProcessorEditor::updateContextHelp()
{
    constexpr double dwellMs = 700.0;
    const auto mouse = juce::Desktop::getMousePosition();
    const auto local = getLocalPoint(nullptr, mouse);
    auto* component = getLocalBounds().contains(local) ? getComponentAt(local) : nullptr;
    juce::String help;
    auto* helpTarget = component;
    while (helpTarget != nullptr && helpTarget != this)
    {
        help = helpTarget->getProperties()["mbc4ContextHelp"].toString();
        if (help.isNotEmpty()) break;
        helpTarget = helpTarget->getParentComponent();
    }
    if (help.isEmpty()) helpTarget = nullptr;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto moved = mouse.getDistanceFrom(lastMousePosition) > 2.0;
    if (helpTarget != hoverHelpTarget || moved)
    {
        hoverHelpTarget = helpTarget;
        lastMousePosition = mouse;
        hoverHelpStartedMs = now;
        activeHelpText.clear();
        contextHeader.setHelpText({});
        return;
    }
    if (helpTarget != nullptr && now - hoverHelpStartedMs >= dwellMs && activeHelpText != help)
    {
        activeHelpText = help;
        contextHeader.setHelpText(help);
    }
}

void PonteMC2000AudioProcessorEditor::timerCallback()
{
    updateBandCountLayout();
    updateContextHelp();
    for (auto& band : bands) band->repaint();
    crossoverPlot.repaint();
    compressionPlot.repaint();
    outputMeter.repaint();
}

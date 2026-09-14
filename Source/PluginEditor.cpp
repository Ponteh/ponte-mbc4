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
                    const juce::Justification justification = juce::Justification::centred,
                    const juce::Colour colour = pontedsp::gui::Palette::text())
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(size, juce::Font::bold));
    label.setJustificationType(justification);
    label.setColour(juce::Label::textColourId, colour);
}

void setContextHelp(juce::Component& component, const juce::String& text)
{
    component.getProperties().set("mbc4ContextHelp", text);
}

float meterPosition(const float db) noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
}

constexpr std::array<const char*, 7> linkedSuffixes {
    "gainDb", "thresholdDb", "ratio", "knee", "bite", "attackMs", "releaseMs"
};

} // namespace

ParameterKnob::ParameterKnob(juce::AudioProcessorValueTreeState& state,
                             const juce::String& parameterId,
                             const juce::String& caption, const juce::String& suffix,
                             const juce::String& helpText, const int decimalPlaces)
{
    setContextHelp(*this, helpText);
    setComponentID(parameterId);
    configureLabel(name, caption, 10.0f, juce::Justification::centred,
                   pontedsp::gui::Palette::text());
    addAndMakeVisible(name);
    slider.getProperties().set("pontedspKnobActive", false);
    slider.setWantsKeyboardFocus(true);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setDoubleClickReturnValue(true,
        state.getParameterRange(parameterId).convertFrom0to1(
            state.getParameter(parameterId)->getDefaultValue()));
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(decimalPlaces);
    slider.setMouseDragSensitivity(180);
    addAndMakeVisible(slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, parameterId, slider);
    valueDisplay.setEditable(true, true, false);
    valueDisplay.setJustificationType(juce::Justification::centred);
    valueDisplay.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    valueDisplay.setColour(juce::Label::textColourId, pontedsp::gui::Palette::text());
    valueDisplay.setColour(juce::Label::backgroundColourId, pontedsp::gui::Palette::elevated());
    valueDisplay.setColour(juce::Label::outlineColourId, pontedsp::gui::Palette::lime());
    valueDisplay.setColour(juce::Label::textWhenEditingColourId, pontedsp::gui::Palette::text());
    valueDisplay.setColour(juce::Label::backgroundWhenEditingColourId, pontedsp::gui::Palette::elevated());
    valueDisplay.setTitle(caption + " value");
    valueDisplay.setComponentID(parameterId + ".value");
    valueDisplay.setAlwaysOnTop(true);
    valueDisplay.addMouseListener(this, true);
    valueDisplay.onTextChange = [this, parameter = state.getParameter(parameterId)]
    {
        const auto requested = slider.getValueFromText(valueDisplay.getText());
        if (std::isfinite(requested))
        {
            parameter->beginChangeGesture();
            slider.setValue(requested, juce::sendNotificationSync);
            parameter->endChangeGesture();
        }
        updateValueText();
    };
    slider.onValueChange = [this] { updateValueText(); };
    updateValueText();
    slider.onDragStart = [this]
    {
        dragging = true;
    };
    slider.onDragEnd = [this]
    {
        dragging = false;
    };
    addMouseListener(this, true);
    setValueVisible(false);
    startTimerHz(30);
}

void ParameterKnob::setValueVisible(const bool visible)
{
    const auto show = visible && isShowing() && valueDisplay.getParentComponent() != nullptr;
    if (!show && valueDisplay.isBeingEdited()) valueDisplay.hideEditor(false);
    if (show) positionValueDisplay();
    valueDisplay.setVisible(show);
    if (show) valueDisplay.toFront(false);
    if (static_cast<bool>(slider.getProperties()["pontedspKnobActive"]) != show)
    {
        slider.getProperties().set("pontedspKnobActive", show);
        slider.repaint();
    }
}

void ParameterKnob::updateValueText()
{
    if (!valueDisplay.isBeingEdited())
        valueDisplay.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification);
}

void ParameterKnob::parentHierarchyChanged()
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
    {
        if (valueDisplay.getParentComponent() != editor)
            editor->addChildComponent(valueDisplay);
        positionValueDisplay();
    }
    else if (auto* parent = valueDisplay.getParentComponent())
    {
        valueDisplay.setVisible(false);
        parent->removeChildComponent(&valueDisplay);
    }
}

void ParameterKnob::positionValueDisplay()
{
    if (auto* parent = valueDisplay.getParentComponent())
    {
        const auto knobBounds = parent->getLocalArea(&slider, slider.getLocalBounds());
        constexpr int width = 100, height = 24, gap = 3;
        const auto x = juce::jlimit(0, juce::jmax(0, parent->getWidth() - width),
                                   knobBounds.getCentreX() - width / 2);
        const auto y = juce::jlimit(0, juce::jmax(0, parent->getHeight() - height),
                                   knobBounds.getY() - height - gap);
        valueDisplay.setBounds(x, y, width, height);
    }
}

void ParameterKnob::registerFocus(pontedsp::gui::ControlFocus& focus)
{
    focus.add(*this, [this](bool active)
    {
        setValueVisible(active);
        if (active)
            if (auto* strip = findParentComponentOfClass<BandStrip>())
                if (strip->onInteraction) strip->onInteraction();
    }, &valueDisplay, [this] { return dragging || valueDisplay.isBeingEdited(); });
}

void ParameterKnob::mouseDown(const juce::MouseEvent&)
{
    if (auto* strip = findParentComponentOfClass<BandStrip>())
        if (strip->onInteraction) strip->onInteraction();
}

void ParameterKnob::timerCallback()
{
    updateValueText();
}

void ParameterKnob::resized()
{
    constexpr int captionHeight = 15;
    const auto diameter = juce::jmax(0, juce::jmin(getWidth(), getHeight() - captionHeight));
    auto group = getLocalBounds().withSizeKeepingCentre(getWidth(), diameter + captionHeight);
    slider.setBounds(group.removeFromTop(diameter));
    name.setBounds(group);
    positionValueDisplay();
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
        g.drawFittedText(helpText, area, juce::Justification::centredLeft, 4, 1.0f);
        return;
    }

    auto title = area;
    auto subtitle = title.removeFromBottom(20);
    g.setColour(pontedsp::gui::Palette::text());
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawFittedText("PONTE DSP", title, juce::Justification::centredLeft, 1);
    g.setColour(pontedsp::gui::Palette::mutedText());
    g.setFont(juce::FontOptions(13.0f));
    g.drawFittedText("MBC4", subtitle, juce::Justification::centredLeft, 1);
}

CrossoverField::CrossoverField(juce::AudioProcessorValueTreeState& valueTreeState,
                               const int crossoverIndex)
    : state(valueTreeState), index(crossoverIndex)
{
    setComponentID(pontedsp::mc2000::parameters::crossoverId(index));
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
    const auto outline = pontedsp::gui::Palette::outline();
    g.setColour(static_cast<bool>(getProperties()["pontedspControlActive"])
        ? outline.interpolatedWith(pontedsp::gui::Palette::lime(), 0.25f) : outline);
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
    auto area = getLocalBounds().toFloat().reduced(2.0f, 1.0f);
    const auto row = area.getHeight() / 3.0f;
    const std::array<float, 3> positions {
        meterPosition(snapshot.inputDb), meterPosition(snapshot.outputDb),
        juce::jlimit(0.0f, 1.0f, snapshot.gainReductionDb / 48.0f)
    };
    const std::array<const char*, 3> labels { "IN", "OUT", "GR" };
    for (int i = 0; i < 3; ++i)
    {
        auto line = area.removeFromTop(row);
        g.setColour(pontedsp::gui::Palette::text());
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(labels[static_cast<std::size_t>(i)], line.removeFromLeft(27.0f),
                   juce::Justification::centredLeft);
        const auto scaleHeight = 12.0f;
        auto scale = line.removeFromBottom(scaleHeight);
        auto bar = line.withSizeKeepingCentre(line.getWidth(),
            juce::jmin(6.0f, line.getHeight()));
        g.setColour(pontedsp::gui::Palette::elevated());
        g.fillRoundedRectangle(bar, 2.0f);
        g.setColour(i == 2 ? pontedsp::gui::Palette::danger()
                           : bandColours[static_cast<std::size_t>(band)]);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth()
            * positions[static_cast<std::size_t>(i)]), 2.0f);

        g.setColour(pontedsp::gui::Palette::divider());
        g.drawHorizontalLine(juce::roundToInt(scale.getY()), scale.getX(), scale.getRight());
        g.setColour(pontedsp::gui::Palette::text());
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        for (int tick = 0; tick < 5; ++tick)
        {
            const auto x = juce::jmap(static_cast<float>(tick), 0.0f, 4.0f,
                                      scale.getX(), scale.getRight());
            g.drawVerticalLine(juce::roundToInt(x), scale.getY(), scale.getY() + 2.0f);
            const auto value = i == 2 ? -12 * tick : -48 + 12 * tick;
            const auto textX = juce::jlimit(scale.getX(), scale.getRight() - 26.0f, x - 13.0f);
            g.drawText(juce::String(value), juce::roundToInt(textX),
                       juce::roundToInt(scale.getY() + 1.0f), 26,
                       juce::roundToInt(scaleHeight),
                       juce::Justification::centred);
        }
    }
}

BandStrip::BandStrip(PonteMC2000AudioProcessor& p, const int bandIndex)
    : processor(p), band(bandIndex),
      gain(p.state, pontedsp::mc2000::parameters::bandId(band, "gainDb"), "GAIN", " dB",
           "Add post-compression makeup gain to this band.", 1),
      threshold(p.state, pontedsp::mc2000::parameters::bandId(band, "thresholdDb"), "THRESH", " dB",
                "Set the peak level where compression starts for this band.", 1),
      ratio(p.state, pontedsp::mc2000::parameters::bandId(band, "ratio"), "RATIO", " :1",
            "Set how strongly signals above the threshold are compressed.", 2),
      knee(p.state, pontedsp::mc2000::parameters::bandId(band, "knee"), "KNEE", {},
           "Shape the transition around threshold: undershoot, hard knee, overshoot or tail.", 2),
      bite(p.state, pontedsp::mc2000::parameters::bandId(band, "bite"), "BITE", {},
           "Let more transient detail pass while preserving steady-state compression.", 2),
      attack(p.state, pontedsp::mc2000::parameters::bandId(band, "attackMs"), "ATTACK", " ms",
             "Set how quickly gain reduction reacts to a rising signal.", 2),
      release(p.state, pontedsp::mc2000::parameters::bandId(band, "releaseMs"), "RELEASE", " ms",
              "Set how quickly gain reduction returns after the signal falls.", 1),
      meter(p, band)
{
    configureLabel(title, "BAND " + juce::String(band + 1), 13.0f,
                   juce::Justification::centredLeft);
    title.setColour(juce::Label::textColourId, bandColours[static_cast<std::size_t>(band)]);
    configureLabel(algorithmLabel, "ALGORITHM", 9.0f, juce::Justification::centredLeft,
                   pontedsp::gui::Palette::text());
    addAndMakeVisible(title);
    for (auto* component : std::array<juce::Component*, 12> {
        &enabled, &solo, &gain, &threshold, &ratio, &knee, &bite, &attack, &release,
        &algorithmLabel, &timeConstant, &meter })
        addAndMakeVisible(*component);
    enabled.setClickingTogglesState(true);
    solo.setClickingTogglesState(true);
    enabled.setComponentID(pontedsp::mc2000::parameters::bandId(band, "enabled"));
    solo.setComponentID(pontedsp::mc2000::parameters::bandId(band, "solo"));
    enabled.onClick = [this]
    {
        // IN is the saved bypass state. SOLO only overrides its presentation/routing.
        const auto count = juce::jlimit(2, 4, static_cast<int>(processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandCount)->load()) + 2);
        for (int i = 0; i < count; ++i)
            if (processor.state.getRawParameterValue(
                pontedsp::mc2000::parameters::bandId(i, "solo"))->load() > 0.5f) return;
        auto* parameter = processor.state.getParameter(
            pontedsp::mc2000::parameters::bandId(band, "enabled"));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(enabled.getToggleState() ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    };
    timeConstant.addItemList({ "R1", "R2", "AUTO" }, 1);
    setContextHelp(enabled, "Enable this band's compression. IN settings are preserved while SOLO overrides the bands.");
    setContextHelp(solo, "Monitor one or more bands. Releasing the last SOLO restores the saved IN settings.");
    setContextHelp(timeConstant, "Choose Pure Peak R1, adaptive release R2, or program-dependent Auto timing.");
    setContextHelp(meter, "Monitor band input, output and gain reduction levels.");
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, pontedsp::mc2000::parameters::bandId(band, "solo"), solo);
    timeConstantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, pontedsp::mc2000::parameters::bandId(band, "tcMode"), timeConstant);
    addMouseListener(this, true);
}

void BandStrip::updateInState(const bool anySolo)
{
    const auto on = !anySolo && processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandId(band, "enabled"))->load() > 0.5f;
    enabled.setToggleState(on, juce::dontSendNotification);
    enabled.setEnabled(!anySolo);
}

void BandStrip::mouseDown(const juce::MouseEvent&)
{
    if (onInteraction) onInteraction();
}

void BandStrip::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&)
{
    if (onInteraction) onInteraction();
}

void BandStrip::focusOfChildComponentChanged(FocusChangeType)
{
    if (hasKeyboardFocus(true) && onInteraction) onInteraction();
}

void BandStrip::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(pontedsp::gui::Palette::surface().withAlpha(activeVisual ? 0.94f : 0.54f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(bandColours[static_cast<std::size_t>(band)]
        .withAlpha(activeVisual ? 0.38f : 0.16f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    g.setColour(pontedsp::gui::Palette::divider());
    for (const auto* knob : { &gain, &ratio, &bite })
    {
        const auto x = static_cast<float>(knob->getRight());
        g.drawVerticalLine(juce::roundToInt(x), 11.0f,
                           static_cast<float>(getHeight() - 11));
    }
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced(8, 5);
    auto identity = area.removeFromLeft(98);
    title.setBounds(identity.removeFromTop(22));
    const auto buttonHeight = juce::jlimit(15, 30, identity.getHeight() / 2);
    enabled.setBounds(identity.removeFromTop(buttonHeight).reduced(2));
    solo.setBounds(identity.removeFromTop(buttonHeight).reduced(2));

    const auto knobWidth = juce::jlimit(58, 82, (area.getWidth() - 280) / 7);
    auto knobArea = area.removeFromLeft(knobWidth * 7);
    auto meterArea = area.reduced(5, 0);
    auto algorithm = meterArea.removeFromTop(
        juce::jlimit(18, 27, meterArea.getHeight() / 3));
    algorithmLabel.setBounds(algorithm.removeFromLeft(82));
    timeConstant.setBounds(algorithm.reduced(2, 0));
    meter.setBounds(meterArea);
    for (auto* knob : std::array<ParameterKnob*, 7> {
        &gain, &threshold, &ratio, &knee, &bite, &attack, &release })
        knob->setBounds(knobArea.removeFromLeft(knobWidth));
}

void BandStrip::setActiveVisual(const bool active)
{
    if (activeVisual == active) return;
    activeVisual = active;
    const auto alpha = active ? 1.0f : 0.38f;
    for (auto* component : std::array<juce::Component*, 10> {
        &title, &gain, &threshold, &ratio, &knee, &bite, &attack, &release,
        &algorithmLabel, &timeConstant })
        component->setAlpha(alpha);
    meter.setAlpha(alpha);
    enabled.setAlpha(1.0f);
    solo.setAlpha(1.0f);
    repaint();
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
    constexpr float left = 34.0f;
    constexpr float right = 8.0f;
    return static_cast<float>(std::log(frequency / 20.0) / std::log(1000.0))
         * std::max(1.0f, static_cast<float>(getWidth()) - left - right) + left;
}

double CrossoverPlot::xToFrequency(const float x) const noexcept
{
    constexpr double left = 34.0;
    constexpr double right = 8.0;
    return 20.0 * std::pow(1000.0, juce::jlimit(0.0, 1.0,
        (static_cast<double>(x) - left) / std::max(1.0, getWidth() - left - right)));
}

bool CrossoverPlot::bandIsAudible(const int requestedBand) const noexcept
{
    const auto count = currentBandCount();
    auto anySolo = false;
    for (int band = 0; band < count; ++band)
        anySolo = anySolo || processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "solo"))->load() > 0.5f;
    if (requestedBand < 0 || requestedBand >= count) return false;
    const auto enabled = processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandId(requestedBand, "enabled"))->load() > 0.5f;
    const auto solo = processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandId(requestedBand, "solo"))->load() > 0.5f;
    return anySolo ? solo : enabled;
}

int CrossoverPlot::bandForFrequency(const double frequency) const noexcept
{
    for (int crossover = 0; crossover < currentBandCount() - 1; ++crossover)
        if (frequency < processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(crossover))->load())
            return crossover;
    return currentBandCount() - 1;
}

void CrossoverPlot::updateSpectrum()
{
    std::array<float, 4096> incoming {};
    const auto count = processor.popSpectrumSamples(incoming.data(),
                                                     static_cast<int>(incoming.size()));
    for (int sample = 0; sample < count; ++sample)
    {
        fftInput[static_cast<std::size_t>(fftInputCount++)] = incoming[static_cast<std::size_t>(sample)];
        if (fftInputCount < fftSize) continue;

        std::fill(fftWork.begin(), fftWork.end(), 0.0f);
        std::copy(fftInput.begin(), fftInput.end(), fftWork.begin());
        fftWindow.multiplyWithWindowingTable(fftWork.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform(fftWork.data());
        for (int bin = 0; bin < fftSize / 2; ++bin)
        {
            const auto db = juce::Decibels::gainToDecibels(
                fftWork[static_cast<std::size_t>(bin)] * (2.0f / fftSize), -100.0f);
            auto& displayed = spectrumDb[static_cast<std::size_t>(bin)];
            displayed = spectrumReady ? 0.72f * displayed + 0.28f * db : db;
        }
        spectrumReady = true;
        std::copy(fftInput.begin() + fftSize / 2, fftInput.end(), fftInput.begin());
        fftInputCount = fftSize / 2;
    }
}

void CrossoverPlot::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const auto plot = area.withTrimmedLeft(34.0f).withTrimmedRight(8.0f)
                          .withTrimmedTop(18.0f).withTrimmedBottom(20.0f);
    g.setColour(pontedsp::gui::Palette::ink().withAlpha(0.92f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setFont(juce::FontOptions(9.0f));
    for (const auto frequency : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
    {
        const auto x = frequencyToX(frequency);
        g.setColour(pontedsp::gui::Palette::outline().withAlpha(0.3f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(pontedsp::gui::Palette::mutedText());
        const auto label = frequency >= 1000.0
            ? juce::String(frequency / 1000.0, 0) + "k"
            : juce::String(static_cast<int>(frequency));
        g.drawText(label, juce::roundToInt(x) - 18, juce::roundToInt(plot.getBottom() + 3.0f),
                   36, 13, juce::Justification::centred);
    }
    for (const auto db : { -48, -36, -24, -12, 0 })
    {
        const auto y = juce::jmap(static_cast<float>(db), 0.0f, -48.0f,
                                  plot.getY(), plot.getBottom());
        g.setColour(pontedsp::gui::Palette::outline().withAlpha(db == 0 ? 0.6f : 0.24f));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        g.setColour(pontedsp::gui::Palette::mutedText());
        g.drawText((db > 0 ? "+" : "") + juce::String(db), 2,
                   juce::roundToInt(y) - 6, 29, 12, juce::Justification::centredRight);
    }

    if (spectrumReady)
    {
        juce::Path spectrum;
        auto drawing = false;
        const auto sampleRate = std::max(1.0, processor.getProcessingSampleRate());
        for (int pixel = 0; pixel <= juce::roundToInt(plot.getWidth()); ++pixel)
        {
            const auto x = plot.getX() + static_cast<float>(pixel);
            const auto frequency = xToFrequency(x);
            if (!bandIsAudible(bandForFrequency(frequency)))
            {
                drawing = false;
                continue;
            }
            const auto bin = juce::jlimit(0.0, static_cast<double>(fftSize / 2 - 1),
                                          frequency * fftSize / sampleRate);
            const auto lower = static_cast<int>(bin);
            const auto upper = std::min(lower + 1, fftSize / 2 - 1);
            const auto mix = static_cast<float>(bin - lower);
            const auto db = juce::jmap(mix,
                spectrumDb[static_cast<std::size_t>(lower)],
                spectrumDb[static_cast<std::size_t>(upper)]);
            const auto y = juce::jmap(juce::jlimit(-48.0f, 0.0f, db),
                                      0.0f, -48.0f, plot.getY(), plot.getBottom());
            if (!drawing) spectrum.startNewSubPath(x, y); else spectrum.lineTo(x, y);
            drawing = true;
        }
        g.setColour(pontedsp::gui::Palette::mutedText().withAlpha(0.4f));
        g.strokePath(spectrum, juce::PathStrokeType(1.0f));
    }
    for (int band = 0; band < currentBandCount(); ++band)
    {
        juce::Path path;
        for (int point = 0; point <= 180; ++point)
        {
            const auto x = plot.getX() + plot.getWidth() * static_cast<float>(point) / 180.0f;
            const auto db = processor.getEngine().getBandMagnitudeDb(band, xToFrequency(x));
            const auto y = juce::jmap(static_cast<float>(juce::jlimit(-48.0, 0.0, db)),
                                      0.0f, -48.0f, plot.getY(), plot.getBottom());
            if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(bandColours[static_cast<std::size_t>(band)]
            .withAlpha(bandIsAudible(band) ? 1.0f : 0.28f));
        g.strokePath(path, juce::PathStrokeType(1.6f));
    }
    for (int crossover = 0; crossover < currentBandCount() - 1; ++crossover)
    {
        const auto frequency = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::crossoverId(crossover))->load();
        const auto x = frequencyToX(frequency);
        g.setColour(pontedsp::gui::Palette::text().withAlpha(0.22f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(pontedsp::gui::Palette::text());
        const auto zeroY = plot.getY();
        g.fillEllipse(x - 3.0f, zeroY - 3.0f, 6.0f, 6.0f);
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

void CompressionPlot::setForegroundBand(const int band)
{
    const auto nextBand = juce::jlimit(0, 3, band);
    if (foregroundBand == nextBand) return;
    foregroundBand = nextBand;
    repaint();
}

void CompressionPlot::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const auto plot = area.withTrimmedLeft(32.0f).withTrimmedRight(8.0f)
                          .withTrimmedTop(21.0f).withTrimmedBottom(20.0f);
    g.setColour(pontedsp::gui::Palette::ink().withAlpha(0.92f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setFont(juce::FontOptions(8.0f));
    for (const auto db : { -48, -36, -24, -12, 0 })
    {
        const auto x = juce::jmap(static_cast<float>(db), -48.0f, 0.0f,
                                  plot.getX(), plot.getRight());
        const auto y = juce::jmap(static_cast<float>(db), -48.0f, 0.0f,
                                  plot.getBottom(), plot.getY());
        g.setColour(pontedsp::gui::Palette::outline().withAlpha(db == 0 ? 0.55f : 0.24f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        g.setColour(pontedsp::gui::Palette::mutedText());
        g.drawText(juce::String(db), juce::roundToInt(x) - 13,
                   juce::roundToInt(plot.getBottom() + 3.0f), 26, 12,
                   juce::Justification::centred);
        g.drawText(juce::String(db), 1, juce::roundToInt(y) - 6, 28, 12,
                   juce::Justification::centredRight);
    }
    g.setColour(pontedsp::gui::Palette::outline().withAlpha(0.6f));
    g.drawLine(plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);
    const auto count = juce::jlimit(2, 4, static_cast<int>(processor.state.getRawParameterValue(
        pontedsp::mc2000::parameters::bandCount)->load()) + 2);
    auto anySolo = false;
    for (int band = 0; band < count; ++band)
        anySolo = anySolo || processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "solo"))->load() > 0.5f;
    const auto front = juce::jlimit(0, count - 1, foregroundBand);
    for (int layer = 0; layer < count; ++layer)
    {
        // Preserve the order of other bands, then paint the edited band on top.
        const auto band = layer == count - 1 ? front : (layer < front ? layer : layer + 1);
        const auto enabled = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "enabled"))->load() > 0.5f;
        const auto solo = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "solo"))->load() > 0.5f;
        const auto active = anySolo ? solo : enabled;
        juce::Path path;
        for (int point = 0; point <= 120; ++point)
        {
            const auto input = -48.0 + 48.0 * static_cast<double>(point) / 120.0;
            const auto output = processor.getEngine().getStaticOutputDb(band, input);
            const auto x = plot.getX() + plot.getWidth() * static_cast<float>(point) / 120.0f;
            const auto y = juce::jmap(static_cast<float>(juce::jlimit(-48.0, 0.0, output)),
                                      -48.0f, 0.0f, plot.getBottom(), plot.getY());
            if (point == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(bandColours[static_cast<std::size_t>(band)].withAlpha(active ? 1.0f : 0.28f));
        g.strokePath(path, juce::PathStrokeType(1.6f));

        if (active)
        {
            const auto meter = processor.getEngine().getBandMeter(band);
            const auto liveInput = juce::jlimit(-48.0f, 0.0f, meter.inputDb);
            const auto liveOutput = juce::jlimit(-48.0f, 0.0f,
                static_cast<float>(processor.getEngine().getStaticOutputDb(band, liveInput)));
            const auto dotX = juce::jmap(liveInput, -48.0f, 0.0f, plot.getX(), plot.getRight());
            const auto dotY = juce::jmap(liveOutput, -48.0f, 0.0f, plot.getBottom(), plot.getY());
            g.setColour(bandColours[static_cast<std::size_t>(band)].darker(0.3f));
            g.fillEllipse(dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
        }
    }
    g.setColour(pontedsp::gui::Palette::text());
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("STATIC I/O", 8, 5, 80, 14, juce::Justification::centredLeft);
}

void OutputMeter::paint(juce::Graphics& g)
{
    const auto levels = processor.getEngine().getOutputMeterDb();
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    g.setColour(pontedsp::gui::Palette::text());
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    auto caption = area.removeFromLeft(80.0f).withTrimmedBottom(12.0f);
    g.drawText("MAIN OUTPUT", caption, juce::Justification::centredLeft);
    area.removeFromLeft(4.0f);
    auto scale = area.removeFromBottom(12.0f);
    for (int channel = 0; channel < 2; ++channel)
    {
        auto row = area.removeFromTop(area.getHeight()
            / static_cast<float>(2 - channel)).reduced(0.0f, 2.0f);
        g.setColour(pontedsp::gui::Palette::elevated());
        g.fillRoundedRectangle(row, 2.0f);
        g.setColour(levels[static_cast<std::size_t>(channel)] > -0.1f
                        ? pontedsp::gui::Palette::danger() : pontedsp::gui::Palette::lime());
        g.fillRoundedRectangle(row.withWidth(row.getWidth()
            * meterPosition(levels[static_cast<std::size_t>(channel)])), 2.0f);
    }
    g.setColour(pontedsp::gui::Palette::divider());
    g.drawHorizontalLine(juce::roundToInt(scale.getY()), scale.getX(), scale.getRight());
    g.setColour(pontedsp::gui::Palette::text());
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    for (int tick = 0; tick < 5; ++tick)
    {
        const auto x = juce::jmap(static_cast<float>(tick), 0.0f, 4.0f,
                                  scale.getX(), scale.getRight());
        g.drawVerticalLine(juce::roundToInt(x), scale.getY(), scale.getY() + 2.0f);
        const auto textX = juce::jlimit(scale.getX(), scale.getRight() - 26.0f, x - 13.0f);
        g.drawText(juce::String(-48 + tick * 12), juce::roundToInt(textX),
                   juce::roundToInt(scale.getY() + 1.0f), 26, 11,
                   juce::Justification::centred);
    }
}

PonteMC2000AudioProcessorEditor::PonteMC2000AudioProcessorEditor(PonteMC2000AudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      inputGain(p.state, pontedsp::mc2000::parameters::inputGain, "INPUT", " dB",
                "Adjust the level feeding the crossover and all compressor bands.", 1),
      outputGain(p.state, pontedsp::mc2000::parameters::outputGain, "OUTPUT", " dB",
                 "Adjust the final level after all processed bands are summed.", 1),
      crossoverPlot(p), compressionPlot(p), outputMeter(p)
{
    setLookAndFeel(&lookAndFeel);
    configureLabel(crossoverLabel, "CROSSOVER", 10.0f, juce::Justification::centredLeft,
                   pontedsp::gui::Palette::text());
    crossoverLabel.setBorderSize(juce::BorderSize<int>(0));
    crossoverLabel.setComponentID("crossoverCaption");
    configureLabel(bandCountLabel, "MODE", 9.0f, juce::Justification::centred,
                   pontedsp::gui::Palette::text());
    configureLabel(linkLabel, "LINK", 9.0f, juce::Justification::centred,
                   pontedsp::gui::Palette::text());
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
        bands[static_cast<std::size_t>(band)]->onInteraction = [this, band]
        {
            compressionPlot.setForegroundBand(band);
        };
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
    const auto initialWidth = processor.editorWidth.load();
    const auto initialHeight = processor.editorHeight.load();
    setResizable(true, true);
    setResizeLimits(1100, 738, 1600, 1100);
    setSize(initialWidth, initialHeight);
    controlFocus = std::make_unique<pontedsp::gui::ControlFocus>(*this);
    const std::function<void(juce::Component&)> registerControls = [this, &registerControls](juce::Component& c)
    {
        if (auto* knob = dynamic_cast<ParameterKnob*>(&c)) knob->registerFocus(*controlFocus);
        else if (auto* field = dynamic_cast<CrossoverField*>(&c))
            controlFocus->add(c, {}, nullptr, [field] { return field->isEditing(); });
        else if (auto* box = dynamic_cast<juce::ComboBox*>(&c))
            controlFocus->add(c, [box](bool active)
            {
                if (active)
                    if (auto* strip = box->findParentComponentOfClass<BandStrip>())
                        if (strip->onInteraction) strip->onInteraction();
            }, nullptr, [box] { return box->isPopupActive(); });
        else if (dynamic_cast<juce::Button*>(&c) != nullptr) controlFocus->add(c);
        else for (auto* child : c.getChildren()) registerControls(*child);
    };
    registerControls(*this);
    updateBandVisualStates();
    startTimerHz(30);
}

PonteMC2000AudioProcessorEditor::~PonteMC2000AudioProcessorEditor()
{
    controlFocus.reset();
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
    juce::ColourGradient gradient(pontedsp::gui::Palette::violet().withAlpha(0.55f),
                                  0.0f, 0.0f, pontedsp::gui::Palette::ink(),
                                  0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillRect(getLocalBounds());
}

void PonteMC2000AudioProcessorEditor::resized()
{
    processor.editorWidth.store(getWidth());
    processor.editorHeight.store(getHeight());
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(54);
    const auto meterWidth = juce::roundToInt((area.getWidth() - 168) * 0.36f) - 10;
    outputMeter.setBounds(header.removeFromRight(meterWidth + 5).withTrimmedRight(5).reduced(0, 1));
    // Fit the header at 1100 px without letting LINK intrude into the meter.
    contextHeader.setBounds(header.removeFromLeft(168));
    header.removeFromLeft(5);
    crossoverLabel.setBounds(header.removeFromLeft(62));
    const auto activeFields = activeBandCount() - 1;
    for (int crossover = 0; crossover < 3; ++crossover)
    {
        auto slot = header.removeFromLeft(70);
        const auto visible = crossover < activeFields;
        crossoverFields[static_cast<std::size_t>(crossover)]->setVisible(visible);
        if (visible)
            crossoverFields[static_cast<std::size_t>(crossover)]->setBounds(
                slot.removeFromLeft(66).reduced(0, 9));
    }
    header.removeFromLeft(4);
    bandCountLabel.setBounds(header.removeFromLeft(34));
    bandCount.setBounds(header.removeFromLeft(88).reduced(0, 9));
    header.removeFromLeft(4);
    linkLabel.setBounds(header.removeFromLeft(28));
    linkMaster.setBounds(header.removeFromLeft(106).reduced(0, 9));

    auto displays = area.removeFromTop(212);
    auto master = displays.removeFromLeft(168).reduced(3);
    phase.setBounds(master.removeFromBottom(32).reduced(8, 2));
    inputGain.setBounds(master.removeFromLeft(master.getWidth() / 2));
    outputGain.setBounds(master);
    auto compression = displays.removeFromRight(juce::roundToInt(displays.getWidth() * 0.36f)).reduced(5);
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
    displayedBandCount = count;
    contextHeader.setBandCount(count);
    resized();
}

void PonteMC2000AudioProcessorEditor::updateLinkedControls()
{
    const auto selectedMaster = juce::jlimit(-1, 3, static_cast<int>(
        processor.state.getRawParameterValue(pontedsp::mc2000::parameters::linkMaster)->load()) - 1);
    std::array<std::array<double, linkedControlCount>, 4> values {};
    for (int band = 0; band < 4; ++band)
        for (int control = 0; control < linkedControlCount; ++control)
            values[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)] =
                processor.state.getRawParameterValue(pontedsp::mc2000::parameters::bandId(
                    band, linkedSuffixes[static_cast<std::size_t>(control)]))->load();

    if (!linkDisplayInitialised || selectedMaster != displayedLinkMaster)
    {
        displayedLinkMaster = selectedMaster;
        linkDisplayInitialised = true;
        previousLinkedValues = values;
        if (selectedMaster >= 0)
            for (int band = 0; band < 4; ++band)
                for (int control = 0; control < linkedControlCount; ++control)
                    linkDisplayOffsets[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)] =
                        values[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)]
                      - values[static_cast<std::size_t>(selectedMaster)][static_cast<std::size_t>(control)];
    }

    if (selectedMaster < 0)
    {
        previousLinkedValues = values;
        return;
    }

    auto masterChanged = false;
    for (int control = 0; control < linkedControlCount; ++control)
        masterChanged = masterChanged || std::abs(
            values[static_cast<std::size_t>(selectedMaster)][static_cast<std::size_t>(control)]
          - previousLinkedValues[static_cast<std::size_t>(selectedMaster)][static_cast<std::size_t>(control)]) > 1.0e-7;

    if (masterChanged)
    {
        for (int band = 0; band < 4; ++band)
        {
            if (band == selectedMaster) continue;
            for (int control = 0; control < linkedControlCount; ++control)
            {
                const auto id = pontedsp::mc2000::parameters::bandId(
                    band, linkedSuffixes[static_cast<std::size_t>(control)]);
                if (auto* parameter = processor.state.getParameter(id))
                {
                    const auto desired = values[static_cast<std::size_t>(selectedMaster)]
                                                [static_cast<std::size_t>(control)]
                                       + linkDisplayOffsets[static_cast<std::size_t>(band)]
                                                           [static_cast<std::size_t>(control)];
                    const auto normalised = parameter->convertTo0to1(static_cast<float>(desired));
                    if (std::abs(parameter->getValue() - normalised) > 1.0e-7f)
                        parameter->setValueNotifyingHost(normalised);
                }
            }
        }
    }
    else
    {
        for (int band = 0; band < 4; ++band)
        {
            if (band == selectedMaster) continue;
            for (int control = 0; control < linkedControlCount; ++control)
                if (std::abs(values[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)]
                           - previousLinkedValues[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)]) > 1.0e-7)
                    linkDisplayOffsets[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)] =
                        values[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)]
                      - values[static_cast<std::size_t>(selectedMaster)][static_cast<std::size_t>(control)];
        }
    }

    const auto masterModeId = pontedsp::mc2000::parameters::bandId(selectedMaster, "tcMode");
    const auto masterMode = processor.state.getRawParameterValue(masterModeId)->load();
    for (int band = 0; band < 4; ++band)
    {
        if (band == selectedMaster) continue;
        if (auto* parameter = processor.state.getParameter(
            pontedsp::mc2000::parameters::bandId(band, "tcMode")))
        {
            const auto normalised = parameter->convertTo0to1(masterMode);
            if (std::abs(parameter->getValue() - normalised) > 1.0e-7f)
                parameter->setValueNotifyingHost(normalised);
        }
    }

    for (int band = 0; band < 4; ++band)
        for (int control = 0; control < linkedControlCount; ++control)
            previousLinkedValues[static_cast<std::size_t>(band)][static_cast<std::size_t>(control)] =
                processor.state.getRawParameterValue(pontedsp::mc2000::parameters::bandId(
                    band, linkedSuffixes[static_cast<std::size_t>(control)]))->load();
}

void PonteMC2000AudioProcessorEditor::updateBandVisualStates()
{
    const auto count = activeBandCount();
    auto anySolo = false;
    for (int band = 0; band < count; ++band)
        anySolo = anySolo || processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "solo"))->load() > 0.5f;
    for (int band = 0; band < 4; ++band)
    {
        bands[static_cast<std::size_t>(band)]->updateInState(anySolo);
        const auto enabled = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "enabled"))->load() > 0.5f;
        const auto solo = processor.state.getRawParameterValue(
            pontedsp::mc2000::parameters::bandId(band, "solo"))->load() > 0.5f;
        bands[static_cast<std::size_t>(band)]->setActiveVisual(
            band < count && (anySolo ? solo : enabled));
    }
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
    updateLinkedControls();
    updateBandCountLayout();
    updateBandVisualStates();
    updateContextHelp();
    crossoverPlot.updateSpectrum();
    for (auto& band : bands) band->repaint();
    crossoverPlot.repaint();
    compressionPlot.repaint();
    outputMeter.repaint();
}

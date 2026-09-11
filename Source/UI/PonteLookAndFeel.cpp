#include "PonteLookAndFeel.h"
#include "PontePalette.h"

namespace pontedsp::gui {

PonteLookAndFeel::PonteLookAndFeel()
{
    setColour(juce::Label::textColourId, Palette::text());
    setColour(juce::Slider::rotarySliderFillColourId, Palette::lime());
    setColour(juce::Slider::rotarySliderOutlineColourId, Palette::outline());
    setColour(juce::Slider::textBoxTextColourId, Palette::text());
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, Palette::elevated());
    setColour(juce::ComboBox::textColourId, Palette::text());
    setColour(juce::ComboBox::outlineColourId, Palette::outline());
    setColour(juce::ComboBox::arrowColourId, Palette::lime());
    setColour(juce::PopupMenu::backgroundColourId, Palette::surface());
    setColour(juce::PopupMenu::textColourId, Palette::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::violet());
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::lime());
    setColour(juce::TextButton::buttonColourId, Palette::elevated());
    setColour(juce::TextButton::buttonOnColourId, Palette::lime());
}

// A thin single-ring indicator (track + active arc) with a flat body and a
// short pointer line, closer to FabFilter/Neutron's minimal knob language
// than a beveled, skeuomorphic control.
void PonteLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float position, float startAngle, float endAngle,
                                        juce::Slider& slider)
{
    auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(width), static_cast<float>(height)).reduced(10.0f);
    const auto radius = juce::jmax(11.0f, juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 3.0f);
    const auto centre = area.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);

    juce::Path track, active;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId).withAlpha(0.55f));
    g.strokePath(track, {2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId)
        .withAlpha(slider.isEnabled() ? 1.0f : 0.35f));
    g.strokePath(active, {2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});

    const auto body = radius - 8.0f;
    g.setColour(Palette::elevated());
    g.fillEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour(Palette::outline());
    g.drawEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.0f);

    juce::Path pointer;
    const auto pointerLength = juce::jmax(4.0f, body * 0.62f);
    pointer.startNewSubPath(0.0f, -body * 0.24f);
    pointer.lineTo(0.0f, -pointerLength);
    g.setColour(slider.isEnabled() ? Palette::lime() : Palette::mutedText());
    g.strokePath(pointer, {1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded},
                juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

// Flat fill, thin outline. No drop shadow: shadows read as dated/skeuomorphic
// against the rest of the flat, low-contrast surface language.
void PonteLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& base, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto colour = button.getToggleState() ? Palette::lime() : base;
    if (highlighted) colour = colour.brighter(button.getToggleState() ? 0.06f : 0.18f);
    if (down) colour = colour.darker(0.18f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(button.getToggleState() ? Palette::lime().darker(0.15f) : Palette::outline());
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

void PonteLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.setColour(button.getToggleState() ? Palette::ink() : Palette::text());
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(8, 3),
                     juce::Justification::centred, 1);
}

void PonteLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down,
                                    int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                         static_cast<float>(height)).reduced(1.0f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(down ? Palette::lime() : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    juce::Path arrow;
    const auto cx = static_cast<float>(width - 16), cy = static_cast<float>(height) * 0.5f;
    arrow.startNewSubPath(cx - 3.5f, cy - 1.8f);
    arrow.lineTo(cx, cy + 1.8f);
    arrow.lineTo(cx + 3.5f, cy - 1.8f);
    g.setColour(Palette::lime());
    g.strokePath(arrow, {1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
}

void PonteLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
}

juce::Slider::SliderLayout PonteLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto bounds = slider.getLocalBounds();
    const auto textHeight = juce::jmin(slider.getTextBoxHeight(),
                                       juce::jmax(0, bounds.getHeight() - 24));
    const auto rotaryHeight = juce::jmax(24, juce::jmin(bounds.getWidth(),
                                                        bounds.getHeight() - textHeight));
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = { 0, 0, bounds.getWidth(), rotaryHeight };
    layout.textBoxBounds = {
        (bounds.getWidth() - juce::jmin(slider.getTextBoxWidth(), bounds.getWidth())) / 2,
        juce::jmax(0, rotaryHeight - 3),
        juce::jmin(slider.getTextBoxWidth(), bounds.getWidth()), textHeight
    };
    return layout;
}

juce::Label* PonteLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    if (static_cast<bool>(slider.getProperties()["mbc4ContextualValue"]))
    {
        const auto visible = static_cast<bool>(slider.getProperties()["mbc4ValueVisible"]);
        label->setAlpha(visible ? 1.0f : 0.0f);
        label->setInterceptsMouseClicks(visible, visible);
    }
    return label;
}

} // namespace pontedsp::gui

#include "PonteLookAndFeel.h"
#include "PontePalette.h"

namespace pontedsp::gui {

PonteLookAndFeel::PonteLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, Palette::lime());
    setColour(juce::Slider::rotarySliderOutlineColourId, Palette::outline());
    setColour(juce::Slider::textBoxTextColourId, Palette::text());
    setColour(juce::Slider::textBoxBackgroundColourId, Palette::ink().withAlpha(0.8f));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, Palette::surface());
    setColour(juce::ComboBox::textColourId, Palette::text());
    setColour(juce::ComboBox::outlineColourId, Palette::outline());
    setColour(juce::ComboBox::arrowColourId, Palette::lime());
    setColour(juce::PopupMenu::backgroundColourId, Palette::surface());
    setColour(juce::PopupMenu::textColourId, Palette::text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::violet());
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::lime());
}

void PonteLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float position, float startAngle, float endAngle,
                                        juce::Slider& slider)
{
    auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(width), static_cast<float>(height)).reduced(9.0f);
    const auto radius = juce::jmax(12.0f, juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 4.0f);
    const auto centre = area.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    juce::Path track, active;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(track, {5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
    g.strokePath(active, {5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
    const auto body = radius - 9.0f;
    juce::ColourGradient gradient(Palette::elevated().brighter(0.18f), centre.x - body,
                                  centre.y - body, Palette::ink(), centre.x + body,
                                  centre.y + body, true);
    g.setGradientFill(gradient);
    g.fillEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour(Palette::outline());
    g.drawEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.25f);
    juce::Path pointer;
    pointer.addRoundedRectangle(-2.0f, -body + 6.0f, 4.0f,
                                juce::jmax(10.0f, body * 0.38f), 2.0f);
    g.setColour(slider.isEnabled() ? Palette::lime() : Palette::mutedText());
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void PonteLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& base, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto colour = button.getToggleState() ? Palette::lime() : base;
    if (highlighted) colour = colour.brighter(0.12f);
    if (down) colour = colour.darker(0.18f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), 8.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(button.getToggleState() ? Palette::lime().brighter(0.2f) : Palette::outline());
    g.drawRoundedRectangle(bounds, 8.0f, 1.25f);
}

void PonteLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
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
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(down ? Palette::lime() : box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 7.0f, 1.25f);
    juce::Path arrow;
    const auto cx = static_cast<float>(width - 18), cy = static_cast<float>(height) * 0.5f;
    arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
    arrow.lineTo(cx, cy + 2.0f);
    arrow.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(Palette::lime());
    g.strokePath(arrow, {2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
}

void PonteLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont(juce::FontOptions(13.0f, juce::Font::bold));
}

} // namespace pontedsp::gui

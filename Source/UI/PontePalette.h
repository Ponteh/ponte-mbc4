#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pontedsp::gui {

struct Palette final
{
    static juce::Colour ink() noexcept       { return juce::Colour(0xff09090d); }
    static juce::Colour surface() noexcept   { return juce::Colour(0xff15131b); }
    static juce::Colour elevated() noexcept  { return juce::Colour(0xff211b2b); }
    static juce::Colour outline() noexcept   { return juce::Colour(0xff4d3b62); }
    static juce::Colour purple() noexcept    { return juce::Colour(0xff6f45a6); }
    static juce::Colour violet() noexcept    { return juce::Colour(0xff43256b); }
    static juce::Colour lime() noexcept      { return juce::Colour(0xffe4ff00); }
    static juce::Colour text() noexcept      { return juce::Colour(0xfff4f2f7); }
    static juce::Colour mutedText() noexcept { return juce::Colour(0xffaca3b7); }
    static juce::Colour danger() noexcept    { return juce::Colour(0xffff5b68); }
};

} // namespace pontedsp::gui

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pontedsp::gui {

// Minimalist dark theme, in the spirit of modern flat pro-audio UIs
// (FabFilter Pro-C 3, iZotope Neutron, McDSP MC404): near-flat dark
// surfaces, thin strokes, a single strong accent colour, and restrained
// use of secondary hues for hierarchy rather than decoration.
struct Palette final
{
    static juce::Colour ink() noexcept        { return juce::Colour(0xff0b0b0e); }
    static juce::Colour surface() noexcept    { return juce::Colour(0xff131318); }
    static juce::Colour elevated() noexcept   { return juce::Colour(0xff1b1b21); }
    static juce::Colour outline() noexcept    { return juce::Colour(0xff2c2a33); }
    static juce::Colour divider() noexcept    { return juce::Colour(0xff201f26); }
    static juce::Colour purple() noexcept     { return juce::Colour(0xff7d5cc0); }
    static juce::Colour violet() noexcept     { return juce::Colour(0xff2a2333); }
    static juce::Colour lime() noexcept       { return juce::Colour(0xffdcff3d); }
    static juce::Colour text() noexcept       { return juce::Colour(0xfff4f2f7); }
    static juce::Colour mutedText() noexcept  { return juce::Colour(0xff8d879a); }
    static juce::Colour danger() noexcept     { return juce::Colour(0xffff5b68); }
};

} // namespace pontedsp::gui

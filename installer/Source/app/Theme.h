#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The brand's colours, fonts and logo: brand.json over these neutral defaults
struct Theme
{
    juce::Colour background { 0xff1c1c1e }, panel { 0xff2c2c2e }, line { 0xff3a3a3c };
    juce::Colour text { 0xffededed }, mutedText { 0xffa1a1a6 };
    juce::Colour accent { 0xff2563eb }, accentText { 0xff60a5fa }, highlight { 0xff93c5fd };
    // Text and ticks drawn on the accent
    juce::Colour onAccent { 0xffffffff };

    // Null: the system font
    juce::Typeface::Ptr body, bodyBold, heading;
    // Null: no logo in the brand
    std::unique_ptr<juce::Drawable> logo;
    // For screens with no plugin vendor to show
    juce::String company;

    bool isDark() const { return background.getPerceivedBrightness() < 0.5f; }

    // Sizes are in points, like CSS pixels
    juce::Font bodyFont (float size, bool bold = false) const;
    juce::Font headingFont (float size) const;

    // From the brand files built into the app
    static Theme load();
};

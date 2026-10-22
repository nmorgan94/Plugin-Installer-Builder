#pragma once

#include "Theme.h"

class BrandLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit BrandLookAndFeel (const Theme&);

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

private:
    const Theme& theme;
};

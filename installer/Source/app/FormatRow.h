#pragma once

#include "Theme.h"

// One format: name, detail line and a mark; clickable only with a tick box
class FormatRow final : public juce::Button
{
public:
    enum class Mark { tickBox, done, none };

    FormatRow (const Theme&, const juce::String& formatName);

    void setDetail (const juce::String& text, juce::Colour colour);
    void setMark (Mark);

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    const Theme& theme;
    juce::String detail;
    juce::Colour detailColour;
    Mark mark = Mark::tickBox;
};

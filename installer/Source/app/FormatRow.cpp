#include "FormatRow.h"

namespace
{
    void drawTick (juce::Graphics& g, juce::Rectangle<float> area, float thickness)
    {
        juce::Path tick;
        tick.startNewSubPath (area.getRelativePoint (0.2f, 0.52f));
        tick.lineTo (area.getRelativePoint (0.4f, 0.72f));
        tick.lineTo (area.getRelativePoint (0.8f, 0.3f));

        g.strokePath (tick, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

FormatRow::FormatRow (const Theme& brand, const juce::String& formatName)
    : Button (formatName), theme (brand), detailColour (brand.mutedText)
{
    setClickingTogglesState (true);
}

void FormatRow::setDetail (const juce::String& text, juce::Colour colour)
{
    detail = text;
    detailColour = colour;
    repaint();
}

void FormatRow::setMark (Mark newMark)
{
    mark = newMark;
    setInterceptsMouseClicks (mark == Mark::tickBox, false);
    setWantsKeyboardFocus (mark == Mark::tickBox);
    repaint();
}

void FormatRow::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto area = getLocalBounds().toFloat();

    if (mark == Mark::done)
    {
        g.setColour (theme.accentText);
        drawTick (g, area.removeFromLeft (18.0f).withSizeKeepingCentre (18.0f, 18.0f), 2.0f);
        area.removeFromLeft (14.0f);
    }
    else if (mark == Mark::tickBox)
    {
        const auto box = area.removeFromRight (18.0f).withSizeKeepingCentre (18.0f, 18.0f);
        area.removeFromRight (12.0f);

        if (getToggleState())
        {
            g.setColour (highlighted ? theme.accent.brighter (0.1f) : theme.accent);
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (theme.onAccent);
            drawTick (g, box.reduced (2.0f), 2.0f);
        }
        else
        {
            g.setColour (theme.mutedText.withAlpha (highlighted ? 0.9f : 0.6f));
            g.drawRoundedRectangle (box.reduced (0.75f), 4.0f, 1.5f);
        }
    }

    auto text = area.withSizeKeepingCentre (area.getWidth(), 38.0f);

    g.setColour (theme.text);
    g.setFont (theme.bodyFont (15.0f, true));
    g.drawText (getButtonText(), text.removeFromTop (20.0f), juce::Justification::centredLeft, true);

    text.removeFromTop (2.0f);
    g.setColour (detailColour);
    g.setFont (theme.bodyFont (12.0f));
    g.drawText (detail, text, juce::Justification::centredLeft, true);
}

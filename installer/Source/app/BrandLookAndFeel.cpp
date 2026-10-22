#include "BrandLookAndFeel.h"

namespace
{
    bool isTextOnly (juce::Button& button)
    {
        return button.findColour (juce::TextButton::buttonColourId).isTransparent();
    }
}

BrandLookAndFeel::BrandLookAndFeel (const Theme& t)
    : LookAndFeel_V4 ({ t.background, t.background, t.panel, t.line, t.text,
                        t.accent, t.onAccent, t.accent, t.text }),
      theme (t)
{
    setColour (juce::TextButton::buttonColourId, theme.accent);
    setColour (juce::TextButton::textColourOffId, theme.onAccent);
    setColour (juce::HyperlinkButton::textColourId, theme.accentText);
}

juce::Typeface::Ptr BrandLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    // Fonts made without a typeface get the brand's body font
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        if (auto typeface = font.isBold() ? theme.bodyBold : theme.body)
            return typeface;

    return LookAndFeel_V4::getTypefaceForFont (font);
}

juce::Font BrandLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    return theme.bodyFont (14.0f, ! isTextOnly (button));
}

void BrandLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                             bool highlighted, bool down)
{
    // Transparent buttons are just their text
    if (background.isTransparent())
        return;

    auto colour = button.isEnabled() ? background : background.withMultipliedAlpha (0.4f);

    if (down || highlighted)
        colour = colour.brighter (down ? 0.2f : 0.1f);

    const auto bounds = button.getLocalBounds().toFloat();
    g.setColour (colour);
    g.fillRoundedRectangle (bounds, bounds.getHeight() / 2);
}

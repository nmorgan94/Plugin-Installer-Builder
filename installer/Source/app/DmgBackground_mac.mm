#include "DmgBackground.h"

#import <AppKit/AppKit.h>

namespace
{
    // What dmg/layout.json tells Finder, in points
    struct Layout
    {
        float width = 0, iconX = 0, iconY = 0, iconSize = 0, textSize = 0;
    };

    std::optional<Layout> readLayout (const juce::File& file)
    {
        const auto json = juce::JSON::parse (file);
        const Layout layout { json["width"], json["iconX"], json["iconY"], json["iconSize"], json["textSize"] };

        if (layout.width <= 0 || layout.iconSize <= 0 || layout.textSize <= 0)
            return std::nullopt;

        return layout;
    }

    // Measured the way Finder draws its labels, so the pill fits them
    float finderLabelWidth (const juce::String& label, float textSize)
    {
        NSDictionary* attributes = @{ NSFontAttributeName: [NSFont systemFontOfSize: textSize] };
        return (float) [[NSString stringWithUTF8String: label.toRawUTF8()] sizeWithAttributes: attributes].width;
    }

    void draw (juce::Graphics& g, const Theme& theme, const juce::String& label, const Layout& layout)
    {
        g.fillAll (theme.background);

        // Logo and company name, centred at the top
        const auto nameFont = theme.headingFont (18.0f);
        const auto nameWidth = theme.company.isEmpty() ? 0.0f
                                                       : juce::GlyphArrangement::getStringWidth (nameFont, theme.company);
        const auto logoSize = theme.logo != nullptr ? 28.0f : 0.0f;
        const auto gap = theme.logo != nullptr && nameWidth > 0.0f ? 10.0f : 0.0f;
        auto header = juce::Rectangle<float> (logoSize + gap + nameWidth, 28.0f).withCentre ({ layout.width / 2.0f, 44.0f });

        if (theme.logo != nullptr)
            theme.logo->drawWithin (g, header.removeFromLeft (logoSize), juce::RectanglePlacement::centred, 1.0f);

        if (nameWidth > 0.0f)
        {
            header.removeFromLeft (gap);
            g.setColour (theme.text);
            g.setFont (nameFont);
            g.drawText (theme.company, header, juce::Justification::centredLeft, false);
        }

        // Finder writes the name in black (Light mode) or white (Dark mode); the accent pill suits both
        // Fits ".app" too, which Finder adds when "Show all filename extensions" is on
        const auto pill = juce::Rectangle<float> (finderLabelWidth (label + ".app", layout.textSize) + 32.0f, 26.0f)
                              .withCentre ({ layout.iconX, layout.iconY + layout.iconSize / 2.0f + 19.0f });

        g.setColour (theme.accent);
        g.fillRoundedRectangle (pill, pill.getHeight() / 2.0f);

        g.setColour (theme.mutedText);
        g.setFont (theme.bodyFont (14.0f));
        g.drawText ("Double-click to install", juce::Rectangle<float> (0.0f, pill.getBottom() + 14.0f, layout.width, 20.0f),
                    juce::Justification::centred, false);
    }

    // Bigger than the window, so enlarging it shows more background
    constexpr int canvasWidth = 1600, canvasHeight = 1000;

    bool writePng (const Theme& theme, const juce::String& label, const Layout& layout, int scale,
                   const juce::File& file)
    {
        juce::Image image (juce::Image::RGB, canvasWidth * scale, canvasHeight * scale, false);

        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            draw (g, theme, label, layout);
        }

        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    }
}

bool writeDmgBackground (const Theme& theme, const juce::String& label, const juce::File& layoutFile,
                         const juce::File& folder)
{
    const auto layout = readLayout (layoutFile);

    return layout.has_value()
        && folder.createDirectory()
        && writePng (theme, label, *layout, 1, folder.getChildFile ("background.png"))
        && writePng (theme, label, *layout, 2, folder.getChildFile ("background@2x.png"));
}

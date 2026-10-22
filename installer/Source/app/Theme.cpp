#include "Theme.h"

#if __has_include ("BrandData.h")
 #include "BrandData.h"
 #define HAS_BRAND_DATA 1
#endif

namespace
{
    // A file from the brand folder by name; empty if the brand has none
    juce::Span<const std::byte> brandFile (const juce::String& name)
    {
       #if HAS_BRAND_DATA
        for (int i = 0; i < BrandData::namedResourceListSize; ++i)
        {
            int size = 0;

            if (name == BrandData::originalFilenames[i])
                if (const auto* data = BrandData::getNamedResource (BrandData::namedResourceList[i], size))
                    return { reinterpret_cast<const std::byte*> (data), (size_t) size };
        }
       #endif

        juce::ignoreUnused (name);
        return {};
    }

    // "#rrggbb", or the fallback if missing or malformed
    juce::Colour parseColour (const juce::var& value, juce::Colour fallback)
    {
        const auto hex = value.toString().trimCharactersAtStart ("#");

        if (hex.length() != 6 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
            return fallback;

        return juce::Colour::fromString ("ff" + hex);
    }

    juce::Typeface::Ptr loadFont (const juce::var& fileName)
    {
        const auto data = brandFile (fileName.toString());
        return data.empty() ? nullptr : juce::Typeface::createSystemTypefaceFor (data);
    }

    // A variable font at this weight; a static font as it is
    juce::Typeface::Ptr atWeight (const juce::Typeface::Ptr& typeface, float weight)
    {
        if (typeface == nullptr)
            return nullptr;

        const juce::FontVariableSetting setting { "wght", weight };

        if (auto weighted = typeface->cloneWithVariableSettings ({ &setting, 1 }))
            return weighted;

        return typeface;
    }

    juce::Font fontOrSystem (const juce::Typeface::Ptr& typeface, float size, bool bold)
    {
        const auto options = typeface != nullptr ? juce::FontOptions (typeface)
                                                 : juce::FontOptions().withStyleFlags (bold ? juce::Font::bold : juce::Font::plain);
        return options.withPointHeight (size);
    }
}

juce::Font Theme::bodyFont (float size, bool bold) const
{
    return fontOrSystem (bold ? bodyBold : body, size, bold);
}

juce::Font Theme::headingFont (float size) const
{
    return fontOrSystem (heading, size, true);
}

Theme Theme::load()
{
    Theme theme;

    const auto json = brandFile ("brand.json");
    const auto brand = juce::JSON::parse (juce::String::fromUTF8 (reinterpret_cast<const char*> (json.data()),
                                                                  (int) json.size()));

    theme.company = brand["company"].toString();

    const auto colours = brand["colours"];
    for (auto [key, colour] : { std::pair { "background", &theme.background }, { "panel", &theme.panel },
                                { "line", &theme.line }, { "text", &theme.text }, { "mutedText", &theme.mutedText },
                                { "accent", &theme.accent }, { "accentText", &theme.accentText },
                                { "highlight", &theme.highlight }, { "onAccent", &theme.onAccent } })
        *colour = parseColour (colours[key], *colour);

    const auto fonts = brand["fonts"];
    const auto body = loadFont (fonts["body"]);
    theme.body = atWeight (body, 400.0f);
    // A variable font makes its own bold; a static one needs its bold file named
    theme.bodyBold = atWeight (fonts["bodyBold"].isVoid() ? body : loadFont (fonts["bodyBold"]), 600.0f);
    theme.heading = atWeight (loadFont (fonts["heading"]), 700.0f);

    if (const auto logo = brandFile ("logo.svg"); ! logo.empty())
        theme.logo = juce::Drawable::createFromImageData (logo.data(), logo.size());

    return theme;
}

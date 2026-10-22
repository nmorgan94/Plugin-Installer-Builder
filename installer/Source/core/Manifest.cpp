#include "Manifest.h"

namespace
{
    std::optional<Manifest::Format> parseFormat (const juce::String& text)
    {
        if (text == "VST3") return Manifest::Format::vst3;
        if (text == "AU")   return Manifest::Format::au;
        return std::nullopt;
    }

    const char* bundleExtension (Manifest::Format format)
    {
        return format == Manifest::Format::vst3 ? ".vst3" : ".component";
    }
}

juce::Result Manifest::load (const juce::File& payloadDir, Manifest& result)
{
    const auto file = payloadDir.getChildFile (fileName);

    if (! file.existsAsFile())
        return juce::Result::fail ("No plugins found in " + payloadDir.getFullPathName());

    juce::var json;
    if (const auto parsed = juce::JSON::parse (file.loadFileAsString(), json); parsed.failed())
        return juce::Result::fail ("Can't read " + file.getFullPathName() + ": " + parsed.getErrorMessage());

    Manifest m;
    if (const auto schema = static_cast<int> (json["schema"]); schema != supportedSchema)
        return juce::Result::fail ("Unsupported installer.json schema: " + juce::String (schema));

    m.product   = json["product"].toString();
    m.vendor    = json["vendor"].toString();
    m.version   = json["version"].toString();
    m.bundleId  = json["bundleId"].toString();
    m.category  = json["category"].toString();
    m.url       = json["url"].toString();
    m.email     = json["email"].toString();
    m.copyright = json["copyright"].toString();

    if (m.product.isEmpty() || m.version.isEmpty())
        return juce::Result::fail ("installer.json has no product or version");

    if (const auto* bundles = json["bundles"].getArray())
    {
        for (const auto& entry : *bundles)
        {
            const auto bundleName = entry["name"].toString();
            const auto format = parseFormat (entry["format"].toString());

            if (! format)
                return juce::Result::fail ("Unknown plugin format: " + entry["format"].toString());

            // A bare bundle name with the right extension, so nothing outside the payload is touched
            if (bundleName.containsAnyOf ("/\\") || bundleName.startsWith (".")
                || ! bundleName.endsWith (bundleExtension (*format)))
                return juce::Result::fail ("Invalid bundle name: " + bundleName);

            if (! payloadDir.getChildFile (bundleName).isDirectory())
                return juce::Result::fail ("Missing " + bundleName + " in " + payloadDir.getFullPathName());

            m.bundles.push_back ({ bundleName, *format });
        }
    }

    if (m.bundles.empty())
        return juce::Result::fail ("installer.json lists no plugins");

    result = std::move (m);
    return juce::Result::ok();
}

juce::String formatDisplayName (Manifest::Format format)
{
    return format == Manifest::Format::vst3 ? "VST3" : "Audio Unit";
}

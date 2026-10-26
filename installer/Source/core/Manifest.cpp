#include "Manifest.h"
#include "Platform.h"
#include "PluginBundle.h"

namespace
{
    std::optional<Manifest::Format> parseFormat (const juce::String& text)
    {
        if (text == "VST3") return Manifest::Format::vst3;
        if (text == "AU")   return Manifest::Format::au;
        return std::nullopt;
    }

    const char* formatName (Manifest::Format format)
    {
        return format == Manifest::Format::vst3 ? "VST3" : "AU";
    }

    const char* bundleExtension (Manifest::Format format)
    {
        return format == Manifest::Format::vst3 ? ".vst3" : ".component";
    }

    std::optional<Manifest::Format> bundleFormat (const juce::File& bundle)
    {
        for (const auto format : { Manifest::Format::vst3, Manifest::Format::au })
            if (bundle.hasFileExtension (bundleExtension (format)))
                return format;

        return std::nullopt;
    }
}

juce::Result Manifest::load (const juce::File& payloadDir, Manifest& result, bool needBundles)
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
    m.name      = json["name"].toString();
    m.vendor    = json["vendor"].toString();
    m.version   = json["version"].toString();
    m.bundleId  = json["bundleId"].toString();
    m.category  = json["category"].toString();
    m.url       = json["url"].toString();
    m.email     = json["email"].toString();
    m.copyright = json["copyright"].toString();

    if (m.product.isEmpty() || m.version.isEmpty() || m.bundleId.isEmpty())
        return juce::Result::fail ("installer.json has no product, version or bundle ID");

    // Uninstallers installed before "name" existed
    if (m.name.isEmpty())
        m.name = m.product;

    if (const auto* bundles = json["bundles"].getArray())
    {
        for (const auto& entry : *bundles)
        {
            const auto bundleName = entry["name"].toString();
            const auto format = parseFormat (entry["format"].toString());

            if (! format)
                return juce::Result::fail ("Unknown plugin format: " + entry["format"].toString());

            if (! platform::supportsFormat (*format))
                return juce::Result::fail (formatDisplayName (*format) + " plugins can't be installed on this computer");

            // A bare bundle name with the right extension, so nothing outside the payload is touched
            if (bundleName.containsAnyOf ("/\\") || bundleName.startsWith (".")
                || ! bundleName.endsWith (bundleExtension (*format)))
                return juce::Result::fail ("Invalid bundle name: " + bundleName);

            if (needBundles && ! payloadDir.getChildFile (bundleName).isDirectory())
                return juce::Result::fail ("Missing " + bundleName + " in " + payloadDir.getFullPathName());

            m.bundles.push_back ({ bundleName, *format });
        }
    }

    if (m.bundles.empty())
        return juce::Result::fail ("installer.json lists no plugins");

    result = std::move (m);
    return juce::Result::ok();
}

juce::Result Manifest::fromBundles (const juce::Array<juce::File>& bundleFiles, Manifest& result)
{
    Manifest m;
    juce::String first;

    const auto keepFirst = [] (juce::String& kept, const juce::String& value)
    {
        if (kept.isEmpty())
            kept = value;
    };

    for (const auto& file : bundleFiles)
    {
        if (! file.isDirectory())
            continue;

        const auto base = file.getFileName();
        const auto format = bundleFormat (file);
        if (! format)
            return juce::Result::fail (base + " isn't a .vst3 or .component");

        PluginBundle bundle;
        if (const auto read = PluginBundle::read (file, bundle); read.failed())
            return read;

        keepFirst (first, base);

        // An empty value matches anything
        const auto checkSame = [&] (const juce::String& field, juce::String& kept, const juce::String& value)
        {
            if (kept.isNotEmpty() && value.isNotEmpty() && kept != value)
                return juce::Result::fail (field + " mismatch: " + base + " " + value + ", " + first + " " + kept);

            keepFirst (kept, value);
            return juce::Result::ok();
        };

        for (const auto& checked : { checkSame ("version", m.version, bundle.version),
                                     checkSame ("name", m.name, bundle.name),
                                     checkSame ("vendor", m.vendor, bundle.vendor),
                                     checkSame ("bundle ID", m.bundleId, bundle.bundleId) })
            if (checked.failed())
                return checked;

        keepFirst (m.category, bundle.category);
        keepFirst (m.url, bundle.url);
        keepFirst (m.email, bundle.email);
        keepFirst (m.copyright, bundle.copyright);

        m.bundles.push_back ({ base, *format });
    }

    if (m.bundles.empty())
        return juce::Result::fail ("No plugin bundles found");

    if (m.name.isEmpty() || m.vendor.isEmpty())
        return juce::Result::fail ("couldn't find the plugin's name and vendor");

    // Windows builds have no Info.plist: use JUCE's default, com.<company>.<name>
    if (m.bundleId.isEmpty())
        m.bundleId = "com." + m.vendor.removeCharacters (" ") + "." + m.name.removeCharacters (" ");

    m.product = m.name.startsWith (m.vendor) ? m.name : m.vendor + " " + m.name;

    // Characters Finder or Windows can't show in a file name
    if (m.product.containsAnyOf ("\\/:*?\"<>|"))
        return juce::Result::fail ("product name '" + m.product + "' can't be used in a file name");

    result = std::move (m);
    return juce::Result::ok();
}

juce::Result Manifest::save (const juce::File& payloadDir) const
{
    juce::Array<juce::var> bundleList;
    for (const auto& bundle : bundles)
    {
        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty ("name", bundle.name);
        entry->setProperty ("format", formatName (bundle.format));
        bundleList.add (entry.get());
    }

    juce::DynamicObject::Ptr json = new juce::DynamicObject();
    json->setProperty ("schema", supportedSchema);
    json->setProperty ("product", product);
    json->setProperty ("name", name);
    json->setProperty ("vendor", vendor);
    json->setProperty ("version", version);
    json->setProperty ("bundleId", bundleId);
    json->setProperty ("category", category);
    json->setProperty ("url", url);
    json->setProperty ("email", email);
    json->setProperty ("copyright", copyright);
    json->setProperty ("bundles", bundleList);

    const auto file = payloadDir.getChildFile (fileName);
    if (! file.replaceWithText (juce::JSON::toString (json.get()) + "\n", false, false, "\n"))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());

    return juce::Result::ok();
}

juce::String formatDisplayName (Manifest::Format format)
{
    return format == Manifest::Format::vst3 ? "VST3" : "Audio Unit";
}

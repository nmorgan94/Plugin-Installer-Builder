#include "PluginBundle.h"

namespace
{
    // Steinberg's moduleinfo.json allows trailing commas, which juce::JSON rejects
    // On UTF-8 bytes, since juce::String's [] walks from the start each time
    juce::String withoutTrailingCommas (const juce::String& text)
    {
        const auto in = text.toStdString();
        std::string result;
        result.reserve (in.size());
        bool inString = false, escaped = false;

        for (size_t i = 0; i < in.size(); ++i)
        {
            const auto c = in[i];

            if (inString)
            {
                if (escaped)        escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"')  inString = false;
            }
            else if (c == '"')
            {
                inString = true;
            }
            else if (c == ',')
            {
                const auto next = in.find_first_not_of (" \t\r\n", i + 1);
                if (next != std::string::npos && (in[next] == '}' || in[next] == ']'))
                    continue;
            }

            result += c;
        }

        return juce::String::fromUTF8 (result.data(), static_cast<int> (result.size()));
    }

    // var's [] asserts on anything but a long enough array
    juce::var firstItem (const juce::var& array)
    {
        const auto* items = array.getArray();
        return items != nullptr && ! items->isEmpty() ? items->getFirst() : juce::var();
    }

    juce::File moduleInfoFile (const juce::File& bundle) { return bundle.getChildFile ("Contents/Resources/moduleinfo.json"); }
    juce::File infoPlistFile (const juce::File& bundle)  { return bundle.getChildFile ("Contents/Info.plist"); }

    // Dicts become objects, arrays arrays, the rest text
    juce::var plistValue (const juce::XmlElement& element)
    {
        if (element.hasTagName ("dict"))
        {
            juce::DynamicObject::Ptr object = new juce::DynamicObject();

            for (const auto* key : element.getChildWithTagNameIterator ("key"))
                if (const auto* value = key->getNextElement(); value != nullptr && key->getAllSubText().isNotEmpty())
                    object->setProperty (key->getAllSubText(), plistValue (*value));

            return object.get();
        }

        if (element.hasTagName ("array"))
        {
            juce::Array<juce::var> items;
            for (const auto* item : element.getChildIterator())
                items.add (plistValue (*item));

            return items;
        }

        return element.getAllSubText();
    }

    // JUCE writes Info.plist as XML; anything else gives void
    juce::var readPlist (const juce::File& file)
    {
        if (const auto xml = juce::parseXML (file); xml != nullptr && xml->hasTagName ("plist"))
            if (const auto* dict = xml->getChildByName ("dict"))
                return plistValue (*dict);

        return {};
    }

    // lipo's names for Mach-O CPU types
    juce::String architectureName (juce::uint32 cpuType, juce::uint32 cpuSubtype)
    {
        switch (cpuType)
        {
            case 0x01000007: return "x86_64";
            case 0x0100000c: return (cpuSubtype & 0xff) == 2 ? "arm64e" : "arm64";
            case 7:          return "i386";
            case 12:         return "arm";
            default:         return "unknown";
        }
    }

    // As lipo -archs lists them
    juce::StringArray architectures (const juce::File& executable)
    {
        juce::FileInputStream in (executable);
        if (! in.openedOk())
            return {};

        juce::StringArray result;
        const auto magic = static_cast<juce::uint32> (in.readIntBigEndian());

        if (magic == 0xcafebabe || magic == 0xcafebabf) // universal, big-endian; ...bf has 64-bit offsets
        {
            const auto count = static_cast<juce::uint32> (in.readIntBigEndian());
            const auto entrySize = magic == 0xcafebabe ? 20 : 32;

            for (juce::uint32 i = 0; i < count && i < 32; ++i)
            {
                if (! in.setPosition (8 + static_cast<juce::int64> (i) * entrySize) || in.isExhausted())
                    break;

                const auto cpuType = static_cast<juce::uint32> (in.readIntBigEndian());
                result.add (architectureName (cpuType, static_cast<juce::uint32> (in.readIntBigEndian())));
            }
        }
        else if (magic == 0xcffaedfe || magic == 0xcefaedfe) // one architecture, little-endian
        {
            const auto cpuType = static_cast<juce::uint32> (in.readInt());
            result.add (architectureName (cpuType, static_cast<juce::uint32> (in.readInt())));
        }

        return result;
    }
}

juce::Result PluginBundle::read (const juce::File& bundle, PluginBundle& result)
{
    const auto base = bundle.getFileName();
    PluginBundle b;

    if (const auto file = moduleInfoFile (bundle); file.existsAsFile())
    {
        juce::var json;
        if (juce::JSON::parse (withoutTrailingCommas (file.loadFileAsString()), json).failed() || ! json.isObject())
            return juce::Result::fail ("can't read " + file.getFullPathName());

        const auto& factory = json["Factory Info"];
        b.name    = json["Name"].toString();
        b.version = json["Version"].toString();
        b.vendor  = factory["Vendor"].toString();
        b.url     = factory["URL"].toString();
        b.email   = factory["E-Mail"].toString();

        if (const auto* classes = json["Classes"].getArray())
        {
            for (const auto& moduleClass : *classes)
            {
                if (moduleClass["Category"].toString() == "Audio Module Class")
                {
                    b.category = firstItem (moduleClass["Sub Categories"]).toString();
                    break;
                }
            }
        }
    }

    if (const auto file = infoPlistFile (bundle); file.existsAsFile())
    {
        const auto plist = readPlist (file);
        if (! plist.isObject())
            return juce::Result::fail (base + ": can't read Info.plist (expected JUCE's XML)");

        const auto plistVersion = plist["CFBundleShortVersionString"].toString();
        if (b.version.isNotEmpty() && b.version != plistVersion)
            return juce::Result::fail (base + ": Info.plist says " + plistVersion + " but moduleinfo.json says " + b.version);

        b.version   = plistVersion;
        b.bundleId  = plist["CFBundleIdentifier"].toString();
        b.copyright = plist["NSHumanReadableCopyright"].toString();

        if (b.bundleId.isEmpty())
            return juce::Result::fail (base + ": Info.plist has no CFBundleIdentifier");

        // Audio Units are named "Vendor: Name"
        const auto component = firstItem (plist["AudioComponents"]);
        if (const auto auName = component["name"].toString(); auName.isNotEmpty())
        {
            const auto colon = auName.indexOf (": ");
            if (b.vendor.isEmpty())   b.vendor = colon < 0 ? auName : auName.substring (0, colon);
            if (b.name.isEmpty())     b.name = colon < 0 ? auName : auName.substring (colon + 2);
            if (b.category.isEmpty()) b.category = component["type"].toString() == "aumu" ? "Instrument" : "Fx";
        }

        const auto archs = architectures (bundle.getChildFile ("Contents/MacOS").getChildFile (plist["CFBundleExecutable"].toString()));
        if (! archs.contains ("x86_64") || ! archs.contains ("arm64"))
            return juce::Result::fail (base + " is not universal (built for: " + (archs.isEmpty() ? juce::String ("unknown") : archs.joinIntoString (" ")) + ")");
    }

    if (b.version.isEmpty())
        return juce::Result::fail (base + ": no version found (no moduleinfo.json or Info.plist)");

    result = std::move (b);
    return juce::Result::ok();
}

// VST3s carry moduleinfo.json on every platform; Audio Units only have Info.plist
juce::String PluginBundle::installedVersion (const juce::File& bundle)
{
    juce::var json;

    if (const auto file = moduleInfoFile (bundle);
        file.existsAsFile()
        && juce::JSON::parse (withoutTrailingCommas (file.loadFileAsString()), json).wasOk()
        && json["Version"].toString().isNotEmpty())
        return json["Version"].toString();

    return readPlist (infoPlistFile (bundle))["CFBundleShortVersionString"].toString().trim();
}

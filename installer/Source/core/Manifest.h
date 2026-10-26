#pragma once

#include <juce_core/juce_core.h>

// installer.json: what a payload contains, written at packaging time by save()
struct Manifest
{
    enum class Format { vst3, au };

    struct Bundle
    {
        juce::String name; // e.g. "Reverb.vst3"
        Format format;
    };

    static constexpr int supportedSchema = 1;
    static constexpr const char* fileName = "installer.json";

    // name: the plugin's own name
    juce::String product, name, vendor, version, bundleId, category, url, email, copyright;
    std::vector<Bundle> bundles;

    // Reads and validates installer.json; needBundles is false for the uninstaller
    static juce::Result load (const juce::File& payloadDir, Manifest& result, bool needBundles = true);

    // Packaging: from one build's .vst3 and .component bundles, which must agree
    static juce::Result fromBundles (const juce::Array<juce::File>& bundles, Manifest& result);

    juce::Result save (const juce::File& payloadDir) const;
};

// "VST3" or "Audio Unit".
juce::String formatDisplayName (Manifest::Format);

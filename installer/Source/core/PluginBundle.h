#pragma once

#include <juce_core/juce_core.h>

// What a plugin bundle says about itself, from moduleinfo.json (VST3) and Info.plist (Mac)
struct PluginBundle
{
    // name: the plugin's own name, e.g. "Reverb"
    juce::String name, vendor, version, bundleId, category, url, email, copyright;

    // For packaging: also checks the two files agree and a Mac build is universal
    static juce::Result read (const juce::File& bundle, PluginBundle& result);

    // For an installed copy, which must always get a status: empty if unreadable
    static juce::String installedVersion (const juce::File& bundle);
};

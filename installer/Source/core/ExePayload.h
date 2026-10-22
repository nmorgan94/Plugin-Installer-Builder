#pragma once

#include <juce_core/juce_core.h>

// Plugins zipped onto the Windows installer by exe/build-exe.sh:
// <exe><zip>PluginPayload1 <zip size, 16 hex digits>[signature]
namespace exePayload
{
    struct Range
    {
        juce::int64 start = 0, size = 0;
    };

    // Where the zip is, or nullopt for a plain exe
    std::optional<Range> find (const juce::File& exe);

    bool extract (const juce::File& exe, const juce::File& folder);

    // For the uninstaller
    bool copyWithoutPayload (const juce::File& exe, const juce::File& destination);
}

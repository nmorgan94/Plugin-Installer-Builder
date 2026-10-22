#pragma once

#include <juce_core/juce_core.h>

// Compares dotted versions numerically, so "1.10.0" is newer than "1.9.2". Returns <0, 0 or >0.
inline int compareVersions (const juce::String& a, const juce::String& b)
{
    const auto partsA = juce::StringArray::fromTokens (a, ".", {});
    const auto partsB = juce::StringArray::fromTokens (b, ".", {});

    for (int i = 0; i < juce::jmax (partsA.size(), partsB.size()); ++i)
    {
        // Missing parts read as 0, so "1.2" equals "1.2.0"
        const auto x = partsA[i].getIntValue();
        const auto y = partsB[i].getIntValue();

        if (x != y)
            return x < y ? -1 : 1;
    }

    return 0;
}

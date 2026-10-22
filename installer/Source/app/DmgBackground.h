#pragma once

#include "Theme.h"

// Writes background.png and @2x into folder, matching dmg/layout.json;
// label: the icon name Finder shows
bool writeDmgBackground (const Theme&, const juce::String& label, const juce::File& layoutFile,
                         const juce::File& folder);

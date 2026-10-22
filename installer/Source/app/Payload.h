#pragma once

#include <juce_core/juce_core.h>

// Where the bundled plugins are; each platform has a Payload_<os> file
juce::File defaultPayloadFolder();

// Tidies up as the app quits, e.g. ejects the DMG it was opened from
void releasePayload();

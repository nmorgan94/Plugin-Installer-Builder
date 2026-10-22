#pragma once

#include <juce_core/juce_core.h>

// Where the bundled plugins are; each platform has a Payload_<os> file
juce::File defaultPayloadFolder();

// Tidies up once the user is finished, e.g. ejects the DMG the app was opened from.
void releasePayload();

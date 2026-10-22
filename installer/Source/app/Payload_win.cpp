#include "Payload.h"

// For now a .plugins folder beside the exe, until the payload is appended to the exe itself
juce::File defaultPayloadFolder()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile (".plugins");
}

void releasePayload() {}

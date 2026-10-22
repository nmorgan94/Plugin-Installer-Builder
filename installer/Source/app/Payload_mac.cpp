#include "Payload.h"

// The DMG holds the app and a hidden .plugins folder side by side
juce::File defaultPayloadFolder()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile (".plugins");
}

void releasePayload()
{
    const auto volume = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getParentDirectory();

    // Only when running from a mounted disk image, e.g. /Volumes/SampleRealm Reverb
    if (volume.getParentDirectory() != juce::File ("/Volumes"))
        return;

    // Ejects once this app has quit; ChildProcess doesn't kill the process when destroyed
    juce::ChildProcess eject;
    eject.start (juce::StringArray { "/bin/sh", "-c", "sleep 1; /usr/bin/hdiutil detach \"$0\" -quiet", volume.getFullPathName() }, 0);
}

#include "Payload.h"

// The DMG holds the app and a hidden .plugins folder side by side
juce::File defaultPayloadFolder()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getSiblingFile (".plugins");
}

void releasePayload()
{
    const auto volume = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getParentDirectory();

    // Only when running from a mounted disk image, e.g. /Volumes/Acme Reverb
    if (volume.getParentDirectory() != juce::File ("/Volumes"))
        return;

    // After the app quits; only a disk image, never a drive the app was copied to
    // (ChildProcess doesn't kill the process when destroyed)
    const auto script = "sleep 1; /usr/bin/hdiutil info | /usr/bin/awk -F'\\t' -v m=\"$0\" '$NF == m { found = 1 } END { exit ! found }'"
                        " && /usr/bin/hdiutil detach \"$0\" -quiet";
    juce::ChildProcess eject;
    eject.start (juce::StringArray { "/bin/sh", "-c", script, volume.getFullPathName() }, 0);
}

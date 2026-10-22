#include "ExePayload.h"
#include "Payload.h"

// Inside a packaged exe, or a .plugins folder beside a bare one
juce::File defaultPayloadFolder()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    if (! exePayload::find (exe))
        return exe.getSiblingFile (".plugins");

    // Deleted when the app quits
    static const juce::TemporaryFile extracted;

    // Never install half-unzipped plugins
    if (! exePayload::extract (exe, extracted.getFile()))
        extracted.getFile().deleteRecursively();

    return extracted.getFile();
}

void releasePayload() {}

#pragma once

#include "InstallResult.h"
#include "Manifest.h"

// Same on every platform, with no UI, so a manager app can reuse it
class PluginInstaller
{
public:
    PluginInstaller (Manifest, juce::File payloadDir, juce::File installRoot);

    const Manifest& getManifest() const { return manifest; }

    juce::File installedLocation (const Manifest::Bundle&) const;

    struct Status
    {
        enum class State { notInstalled, unknownVersion, older, same, newer };

        State state = State::notInstalled;
        juce::String installedVersion;

        // e.g. "Update from 1.2.0"
        juce::String describe() const;
    };

    // How the installed copy compares with the payload's version
    Status status (const Manifest::Bundle&) const;

    // Replaces installed copies; asks for admin rights if needed
    InstallResult install (const std::vector<Manifest::Bundle>&) const;

    // Does the copying in this process. InstallerHelper calls it once it has admin rights.
    InstallResult installHere (const std::vector<Manifest::Bundle>&) const;

    // Asks for admin rights if needed
    InstallResult uninstall (const std::vector<Manifest::Bundle>&) const;

    // Does the removing in this process, like installHere.
    InstallResult uninstallHere (const std::vector<Manifest::Bundle>&) const;

    // The manifest's bundles with these names, or nullopt if any name isn't in it.
    std::optional<std::vector<Manifest::Bundle>> findBundles (const juce::StringArray& names) const;

    juce::File logFile() const;

private:
    InstallResult runAsAdmin (const juce::String& command, const std::vector<Manifest::Bundle>&,
                              const juce::String& action) const;
    // Which app to close; checked before changing anything
    juce::String inUseMessage (const std::vector<Manifest::Bundle>&) const;
    InstallResult fail (const juce::String& reason) const;
    void refreshAudioUnits (bool audioUnitsChanged) const;
    void log (const juce::String&) const;

    Manifest manifest;
    juce::File payloadDir, installRoot;
};

#pragma once

#include "InstallResult.h"
#include "Manifest.h"

// What differs per OS; Platform_mac.mm holds only the macOS password prompt
namespace platform
{
    // Root that install folders sit under: "/" for a real install, a temp folder in tests.
    juce::File defaultInstallRoot();

    // Folder a format installs into, e.g. /Library/Audio/Plug-Ins/VST3.
    juce::File installFolder (Manifest::Format, const juce::File& installRoot);

    // Folder for the installer's log, e.g. /Library/Logs/SampleRealm.
    juce::File logFolder (const juce::String& vendor, const juce::File& installRoot);

    bool hasAdminRights();

    // Runs the bundled InstallerHelper as admin, asking for the password
    InstallResult runHelperAsAdmin (const juce::StringArray& helperArgs, const juce::String& prompt);

    // Copies a bundle exactly to a path that doesn't exist yet.
    bool copyBundle (const juce::File& source, const juce::File& destination);

    // OS housekeeping once a bundle is in place, and once all changes are done.
    void afterInstall (const juce::File& installedBundle);
    void afterChanges (bool audioUnitsChanged);
}

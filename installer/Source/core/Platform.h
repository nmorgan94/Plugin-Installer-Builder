#pragma once

#include "InstallResult.h"
#include "Manifest.h"

// What differs per OS, implemented in Platform_mac.mm and Platform_win.cpp
namespace platform
{
    // Root that install folders sit under: "/" or C:\ for a real install, a temp folder in tests.
    juce::File defaultInstallRoot();

    // Folder a format installs into, e.g. /Library/Audio/Plug-Ins/VST3 or C:\Program Files\Common Files\VST3.
    juce::File installFolder (Manifest::Format, const juce::File& installRoot);

    // Folder for the installer's log, e.g. /Library/Logs/Acme or C:\ProgramData\Acme\Logs.
    juce::File logFolder (const juce::String& vendor, const juce::File& installRoot);

    // Audio Units only exist on Mac
    bool supportsFormat (Manifest::Format);

    bool hasAdminRights();

    // What to tell the user while the admin prompt is up
    juce::String adminPromptHint();

    // Runs this executable's helper command as admin; see CommandLine.h
    InstallResult runHelperAsAdmin (const juce::StringArray& helperArgs, const juce::String& prompt);

    // Plain \n line endings for the scripts reading a command's output; nothing on Mac
    void useBinaryOutput();

    // Apps with the bundle open, e.g. "Ableton Live 12"; always empty on Mac
    juce::StringArray appsUsing (const juce::File& bundle);

    // Copies a bundle exactly to a path that doesn't exist yet.
    bool copyBundle (const juce::File& source, const juce::File& destination);

    // OS housekeeping once a bundle is in place, and once all changes are done.
    void afterInstall (const juce::File& installedBundle);
    void afterChanges (bool audioUnitsChanged);

    // Windows' Apps & Features entry and uninstaller; nothing on Mac
    bool registerProduct (const Manifest&, const juce::File& payloadDir, const juce::File& installRoot);

    // Removes them once none of the product's plugins are left
    void unregisterProduct (const Manifest&, const juce::File& installRoot);
}

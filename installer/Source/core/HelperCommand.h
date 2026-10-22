#pragma once

#include "InstallResult.h"

// Flags the Windows exe gives copies of itself
namespace commandLine
{
    constexpr auto helper = "--helper";       // --helper <error-file> <command>...: the elevated re-run
    constexpr auto uninstall = "--uninstall"; // Apps & Features' Uninstall
}

// Runs with admin rights: in InstallerHelper (Mac) or the --helper run (Windows)
// args: install|uninstall <payload-dir> <install-root> <bundle>...
InstallResult runHelperCommand (const juce::StringArray& args);

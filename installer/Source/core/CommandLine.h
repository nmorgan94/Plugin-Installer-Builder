#pragma once

#include "InstallResult.h"

#include <optional>

// Flags the Windows exe gives copies of itself
namespace commandLine
{
    constexpr auto helper = "--helper";       // --helper <error-file> <command>...: the elevated re-run
    constexpr auto uninstall = "--uninstall"; // Apps & Features' Uninstall
}

// Runs the command in args[0] before any UI starts (see commands in CommandLine.cpp) and returns its
// exit code; nothing if it isn't one, so the window opens
std::optional<int> runCommandLine (const juce::StringArray& args);

#pragma once

#include <juce_core/juce_core.h>

struct InstallResult
{
    enum class Outcome { done, cancelled, failed };

    Outcome outcome = Outcome::failed;
    juce::String message; // why it failed
};

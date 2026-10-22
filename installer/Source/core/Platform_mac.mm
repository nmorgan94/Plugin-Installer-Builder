#include "Platform.h"

#import <Foundation/Foundation.h>

// JUCE has no admin-rights API; AppleScript's "with administrator privileges" prompt names this app
namespace
{
    juce::String shellQuote (const juce::String& text)
    {
        return "'" + text.replace ("'", "'\\''") + "'";
    }

    juce::String appleScriptString (const juce::String& text)
    {
        return "\"" + text.replace ("\\", "\\\\").replace ("\"", "\\\"") + "\"";
    }

    // Copied into the app bundle by CMake
    juce::File helperExecutable()
    {
        return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
            .getChildFile ("Contents/Helpers/InstallerHelper");
    }
}

namespace platform
{
    InstallResult runHelperAsAdmin (const juce::StringArray& helperArgs, const juce::String& prompt)
    {
        const auto helper = helperExecutable();

        if (! helper.existsAsFile())
            return { InstallResult::Outcome::failed, "This installer is incomplete (InstallerHelper is missing)." };

        juce::StringArray quoted { shellQuote (helper.getFullPathName()) };
        for (const auto& arg : helperArgs)
            quoted.add (shellQuote (arg));

        const auto source = "do shell script " + appleScriptString (quoted.joinIntoString (" "))
                          + " with prompt " + appleScriptString (prompt)
                          + " with administrator privileges";

        @autoreleasepool
        {
            NSString* scriptSource = @(source.toRawUTF8());
            if (scriptSource == nil)
                return { InstallResult::Outcome::failed, "Couldn't prepare the install command." };

            auto* script = [[NSAppleScript alloc] initWithSource: (NSString* _Nonnull) scriptSource];
            NSDictionary* error = nil;
            const bool succeeded = [script executeAndReturnError: &error] != nil;
           #if ! __has_feature (objc_arc)
            [script release];
           #endif

            if (succeeded)
                return { InstallResult::Outcome::done, {} };

            // -128 is AppleScript's "user cancelled"
            if ([[error objectForKey: NSAppleScriptErrorNumber] intValue] == -128)
                return { InstallResult::Outcome::cancelled, {} };

            // The helper's error output, e.g. "Couldn't copy Reverb.vst3 to ..."
            const auto* text = (NSString*) [error objectForKey: NSAppleScriptErrorMessage];
            return { InstallResult::Outcome::failed,
                     text != nil ? juce::String::fromUTF8 ([text UTF8String]) : juce::String ("Unknown error") };
        }
    }
}

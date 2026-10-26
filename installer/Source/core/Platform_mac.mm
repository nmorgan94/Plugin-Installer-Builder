#include "Platform.h"

#import <Foundation/Foundation.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

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
}

namespace platform
{
    juce::File defaultInstallRoot()
    {
        return juce::File ("/");
    }

    juce::File installFolder (Manifest::Format format, const juce::File& installRoot)
    {
        return installRoot.getChildFile (format == Manifest::Format::vst3 ? "Library/Audio/Plug-Ins/VST3"
                                                                          : "Library/Audio/Plug-Ins/Components");
    }

    juce::File logFolder (const juce::String& vendor, const juce::File& installRoot)
    {
        return installRoot.getChildFile ("Library/Logs").getChildFile (vendor);
    }

    bool supportsFormat (Manifest::Format)
    {
        return true;
    }

    bool hasAdminRights()
    {
        return geteuid() == 0;
    }

    juce::String adminPromptHint()
    {
        return "Enter your password when asked.";
    }

    // JUCE has no admin-rights API; AppleScript's "with administrator privileges" prompt names this app
    InstallResult runHelperAsAdmin (const juce::StringArray& helperArgs, const juce::String& prompt)
    {
        // The app itself, which runs commands before any UI starts
        const auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

        juce::StringArray quoted { shellQuote (self.getFullPathName()) };
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

            // The command's error output, e.g. "Couldn't copy Reverb.vst3 to ..."
            const auto* text = (NSString*) [error objectForKey: NSAppleScriptErrorMessage];
            return { InstallResult::Outcome::failed,
                     text != nil ? juce::String::fromUTF8 ([text UTF8String]) : juce::String ("Unknown error") };
        }
    }

    void useBinaryOutput() {}

    // Safe to replace: a DAW keeps the old copy until it restarts
    juce::StringArray appsUsing (const juce::File&)
    {
        return {};
    }

    bool copyBundle (const juce::File& source, const juce::File& destination)
    {
        // On Mac JUCE copies a bundle as one item, keeping its links, permissions and signature
        return source.copyFileTo (destination);
    }

    void afterInstall (const juce::File& bundle)
    {
        const auto asRoot = hasAdminRights();

        const auto tidy = [asRoot] (const juce::File& f)
        {
            const auto* path = f.getFullPathName().toRawUTF8();

            // Copies from a downloaded DMG inherit its quarantine flag, which can stop hosts loading them
            removexattr (path, "com.apple.quarantine", XATTR_NOFOLLOW);

            // Like a .pkg install: owned by root:wheel, writable only by root, readable by everyone
            struct stat info;
            if (asRoot && lstat (path, &info) == 0)
            {
                lchown (path, 0, 0);

                if (! S_ISLNK (info.st_mode))
                {
                    auto mode = static_cast<mode_t> ((info.st_mode & 07777 & ~(S_IWGRP | S_IWOTH))
                                                     | S_IRUSR | S_IRGRP | S_IROTH);
                    if (S_ISDIR (info.st_mode) || (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)
                        mode = static_cast<mode_t> (mode | S_IXUSR | S_IXGRP | S_IXOTH);

                    chmod (path, mode);
                }
            }
        };

        tidy (bundle);
        for (const auto& entry : juce::RangedDirectoryIterator (bundle, true, "*", juce::File::findFilesAndDirectories,
                                                                juce::File::FollowSymlinks::no))
            tidy (entry.getFile());
    }

    void afterChanges (bool audioUnitsChanged)
    {
        // So Logic and other hosts see new, updated or removed Audio Units without a restart
        if (! audioUnitsChanged)
            return;

        juce::ChildProcess killall;
        if (killall.start ("/usr/bin/killall -9 AudioComponentRegistrar", 0))
            killall.waitForProcessToFinish (5000);
    }

    // Mac has no installed-programs list
    bool registerProduct (const Manifest&, const juce::File&, const juce::File&) { return true; }
    void unregisterProduct (const Manifest&, const juce::File&) {}
}

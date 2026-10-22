#include "Platform.h"

#if JUCE_MAC
 #include <sys/stat.h>
 #include <sys/xattr.h>
 #include <unistd.h>
#endif

namespace platform
{
   #if JUCE_MAC
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

    bool hasAdminRights()
    {
        return geteuid() == 0;
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
   #endif
}

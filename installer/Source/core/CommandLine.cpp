#include "CommandLine.h"
#include "ExePayload.h"
#include "Platform.h"
#include "PluginInstaller.h"

#include <iostream>

namespace
{
    juce::File argumentFile (const juce::String& path)
    {
        return juce::File::getCurrentWorkingDirectory().getChildFile (path);
    }

    int fail (const juce::String& message)
    {
        std::cerr << message << std::endl;
        return 1;
    }

    // KEY=value lines, which the build scripts read
    int printBuild (const Manifest& manifest)
    {
        std::cout << "PRODUCT_NAME=" << manifest.product << "\n"
                  << "PLUGIN_NAME=" << manifest.name << "\n"
                  << "VERSION=" << manifest.version << "\n"
                  << "BUNDLE_ID=" << manifest.bundleId << std::endl;
        return 0;
    }

    // Runs with admin rights: as root (Mac) or in the --helper run (Windows)
    InstallResult runHelperCommand (const juce::StringArray& args)
    {
        const auto failed = [] (const juce::String& message) { return InstallResult { InstallResult::Outcome::failed, message }; };
        const auto command = args[0];

        if ((command != "install" && command != "uninstall") || args.size() < 4)
            return failed ("Usage: install|uninstall <payload-dir> <install-root> <bundle>...");

        const auto payloadDir = argumentFile (args[1]);

        // Re-checks installer.json, so bundle names are never trusted blindly
        Manifest manifest;
        if (const auto result = Manifest::load (payloadDir, manifest, command == "install"); result.failed())
            return failed (result.getErrorMessage());

        const PluginInstaller installer (std::move (manifest), payloadDir, argumentFile (args[2]));
        const juce::StringArray names (args.begin() + 3, args.size() - 3);
        const auto bundles = installer.findBundles (names);

        if (! bundles)
            return failed ("Not in installer.json: " + names.joinIntoString (", "));

        return command == "install" ? installer.installHere (*bundles) : installer.uninstallHere (*bundles);
    }

    // Each command gets the whole argument list, its own name first

    int statusCommand (const juce::StringArray& args)
    {
        const auto payloadDir = argumentFile (args[1]);
        Manifest manifest;

        if (const auto result = Manifest::load (payloadDir, manifest); result.failed())
            return fail (result.getErrorMessage());

        const PluginInstaller installer (std::move (manifest), payloadDir,
                                         args.size() > 2 ? argumentFile (args[2]) : platform::defaultInstallRoot());
        const auto& m = installer.getManifest();
        std::cout << m.product << " " << m.version << " (" << m.bundleId << ")" << std::endl;

        for (const auto& bundle : m.bundles)
            std::cout << formatDisplayName (bundle.format).paddedRight (' ', 11) << bundle.name.paddedRight (' ', 20)
                      << installer.installedLocation (bundle).getParentDirectory().getFullPathName()
                      << ": " << installer.status (bundle).describe() << std::endl;

        return 0;
    }

    // Errors go to stderr, which the Mac password prompt's AppleScript shows the app
    int installCommand (const juce::StringArray& args)
    {
        const auto result = runHelperCommand (args);
        return result.outcome == InstallResult::Outcome::done ? 0 : fail (result.message);
    }

    int extractCommand (const juce::StringArray& args)
    {
        return exePayload::extract (argumentFile (args[1]), argumentFile (args[2])) ? 0 : fail ("No plugins found in " + args[1]);
    }

    int infoCommand (const juce::StringArray& args)
    {
        juce::Array<juce::File> bundles;
        for (int i = 1; i < args.size(); ++i)
            bundles.add (argumentFile (args[i]));

        Manifest manifest;
        const auto result = Manifest::fromBundles (bundles, manifest);
        return result.wasOk() ? printBuild (manifest) : fail (result.getErrorMessage());
    }

    int manifestCommand (const juce::StringArray& args)
    {
        const auto dir = argumentFile (args[1]);
        juce::Array<juce::File> bundles;

        for (const auto* pattern : { "*.vst3", "*.component" })
        {
            auto found = dir.findChildFiles (juce::File::findDirectories, false, pattern);
            found.sort();
            bundles.addArray (found);
        }

        if (bundles.isEmpty())
            return fail ("No plugin bundles in " + dir.getFullPathName());

        Manifest manifest;
        auto result = Manifest::fromBundles (bundles, manifest);
        if (result.wasOk())
            result = manifest.save (dir);

        return result.wasOk() ? printBuild (manifest) : fail (result.getErrorMessage());
    }

    int packExeCommand (const juce::StringArray& args)
    {
        const auto result = exePayload::append (argumentFile (args[1]), argumentFile (args[2]), argumentFile (args[3]));
        return result.wasOk() ? 0 : fail (result.getErrorMessage());
    }

    struct Command
    {
        const char* name;
        const char* arguments;
        const char* about;
        int minArguments, maxArguments;
        int (*run) (const juce::StringArray& args);
    };

    constexpr auto any = std::numeric_limits<int>::max();

    const Command commands[]
    {
        { "status",    "<payload-dir> [<install-root>]",           "what's installed, against installer.json",      1, 2,   statusCommand },
        { "install",   "<payload-dir> <install-root> <bundle>...", "no admin prompt: use a test install root",      3, any, installCommand },
        { "uninstall", "<payload-dir> <install-root> <bundle>...", "",                                               3, any, installCommand },
        { "extract",   "<installer.exe> <dir>",                    "unpacks what pack-exe zipped on",               2, 2,   extractCommand },
        { "info",      "<bundle>...",                              "checks one build's bundles; prints KEY=value",  1, any, infoCommand },
        { "manifest",  "<dir>",                                    "the same, and writes dir/installer.json",       1, 1,   manifestCommand },
        { "pack-exe",  "<installer.exe> <dir> <out.exe>",          "zips dir onto a Windows installer",             3, 3,   packExeCommand },
    };

    int usage (std::ostream& out, int exitCode)
    {
        out << "Usage: " << juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFileName()
            << " <command>, or no command to open the installer window\n";

        for (const auto& command : commands)
            out << "  " << (juce::String (command.name) + " " + command.arguments).paddedRight (' ', 54) << command.about << "\n";

        out << std::flush;
        return exitCode;
    }
}

std::optional<int> runCommandLine (const juce::StringArray& args)
{
    // The result goes to a file: the elevated run has no console the app could read
    if (args[0] == commandLine::helper && args.size() >= 2)
    {
        const auto result = runHelperCommand (juce::StringArray (args.begin() + 2, args.size() - 2));
        juce::File (args[1]).replaceWithText (result.message);
        return result.outcome == InstallResult::Outcome::done ? 0 : 1;
    }

    if (args[0] == "--help")
    {
        platform::useBinaryOutput();
        return usage (std::cout, 0);
    }

    const auto* command = std::find_if (std::begin (commands), std::end (commands),
                                        [&] (const Command& c) { return args[0] == c.name; });

    // Anything else, e.g. --uninstall or --dmg-background, is for the app
    if (command == std::end (commands))
        return std::nullopt;

    platform::useBinaryOutput();

    const auto count = args.size() - 1;
    if (count < command->minArguments || count > command->maxArguments)
        return usage (std::cerr, 2);

    return command->run (args);
}

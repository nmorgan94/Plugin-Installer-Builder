// Installs for the app (as root on Mac) and doubles as a test CLI; see usage()

#include "Manifest.h"
#include "Platform.h"
#include "PluginInstaller.h"

#include <iostream>

namespace
{
    int usage()
    {
        std::cerr << "Usage: InstallerHelper status <payload-dir> [<install-root>]\n"
                     "       InstallerHelper install|uninstall <payload-dir> <install-root> <bundle>..." << std::endl;
        return 2;
    }

    int status (const PluginInstaller& installer)
    {
        const auto& manifest = installer.getManifest();
        std::cout << manifest.product << " " << manifest.version << " (" << manifest.bundleId << ")" << std::endl;

        for (const auto& bundle : manifest.bundles)
            std::cout << formatDisplayName (bundle.format).paddedRight (' ', 11) << bundle.name.paddedRight (' ', 20)
                      << installer.installedLocation (bundle).getParentDirectory().getFullPathName()
                      << ": " << installer.status (bundle).describe() << std::endl;

        return 0;
    }

    int change (const PluginInstaller& installer, bool installing, const juce::StringArray& names)
    {
        const auto bundles = installer.findBundles (names);
        if (! bundles || bundles->empty())
        {
            std::cerr << "Not in installer.json: " << names.joinIntoString (", ") << std::endl;
            return 1;
        }

        const auto result = installing ? installer.installHere (*bundles) : installer.uninstallHere (*bundles);
        if (result.outcome != InstallResult::Outcome::done)
        {
            std::cerr << result.message << std::endl;
            return 1;
        }

        return 0;
    }
}

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args.size() < 2)
        return usage();

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto command = args[0];
    const auto payloadDir = cwd.getChildFile (args[1]);
    const auto installRoot = args.size() > 2 ? cwd.getChildFile (args[2]) : platform::defaultInstallRoot();

    // Re-checks installer.json, so bundle names are never trusted blindly
    Manifest manifest;
    if (const auto result = Manifest::load (payloadDir, manifest); result.failed())
    {
        std::cerr << result.getErrorMessage() << std::endl;
        return 1;
    }

    const PluginInstaller installer (std::move (manifest), payloadDir, installRoot);

    if (command == "status" && args.size() <= 3)
        return status (installer);

    if ((command == "install" || command == "uninstall") && args.size() >= 4)
        return change (installer, command == "install", juce::StringArray (args.begin() + 3, args.size() - 3));

    return usage();
}

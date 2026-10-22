// Installs for the Mac app as root, and doubles as a test CLI; see usage()

#include "ExePayload.h"
#include "HelperCommand.h"
#include "Manifest.h"
#include "Platform.h"
#include "PluginInstaller.h"

#include <iostream>

namespace
{
    int usage()
    {
        std::cerr << "Usage: InstallerHelper status <payload-dir> [<install-root>]\n"
                     "       InstallerHelper install|uninstall <payload-dir> <install-root> <bundle>...\n"
                     "       InstallerHelper extract <windows-installer.exe> <dir>   (to check what build-exe.sh made)"
                  << std::endl;
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
}

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String::fromUTF8 (argv[i]));

    if (args[0] == "install" || args[0] == "uninstall")
    {
        const auto result = runHelperCommand (args);
        if (result.outcome == InstallResult::Outcome::done)
            return 0;

        std::cerr << result.message << std::endl;
        return 1;
    }

    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (args[0] == "extract" && args.size() == 3)
    {
        if (exePayload::extract (cwd.getChildFile (args[1]), cwd.getChildFile (args[2])))
            return 0;

        std::cerr << "No plugins found in " << args[1] << std::endl;
        return 1;
    }

    if (args[0] != "status" || args.size() < 2 || args.size() > 3)
        return usage();

    const auto payloadDir = cwd.getChildFile (args[1]);
    const auto installRoot = args.size() > 2 ? cwd.getChildFile (args[2]) : platform::defaultInstallRoot();

    Manifest manifest;
    if (const auto result = Manifest::load (payloadDir, manifest); result.failed())
    {
        std::cerr << result.getErrorMessage() << std::endl;
        return 1;
    }

    return status (PluginInstaller (std::move (manifest), payloadDir, installRoot));
}

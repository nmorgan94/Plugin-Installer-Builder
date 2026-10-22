#include "HelperCommand.h"
#include "PluginInstaller.h"

InstallResult runHelperCommand (const juce::StringArray& args)
{
    const auto failed = [] (const juce::String& message) { return InstallResult { InstallResult::Outcome::failed, message }; };
    const auto command = args[0];

    if ((command != "install" && command != "uninstall") || args.size() < 4)
        return failed ("Usage: install|uninstall <payload-dir> <install-root> <bundle>...");

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto payloadDir = cwd.getChildFile (args[1]);

    // Re-checks installer.json, so bundle names are never trusted blindly
    Manifest manifest;
    if (const auto result = Manifest::load (payloadDir, manifest, command == "install"); result.failed())
        return failed (result.getErrorMessage());

    const PluginInstaller installer (std::move (manifest), payloadDir, cwd.getChildFile (args[2]));
    const juce::StringArray names (args.begin() + 3, args.size() - 3);
    const auto bundles = installer.findBundles (names);

    if (! bundles)
        return failed ("Not in installer.json: " + names.joinIntoString (", "));

    return command == "install" ? installer.installHere (*bundles) : installer.uninstallHere (*bundles);
}

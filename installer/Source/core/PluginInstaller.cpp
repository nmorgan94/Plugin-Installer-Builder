#include "PluginInstaller.h"
#include "Platform.h"
#include "Version.h"

namespace
{
    // Steinberg's moduleinfo.json allows trailing commas, which juce::JSON rejects
    juce::String withoutTrailingCommas (const juce::String& text)
    {
        juce::String result;
        bool inString = false, escaped = false;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];

            if (inString)
            {
                if (escaped)        escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"')  inString = false;
            }
            else if (c == '"')
            {
                inString = true;
            }
            else if (c == ',')
            {
                auto next = i + 1;
                while (next < text.length() && juce::CharacterFunctions::isWhitespace (text[next]))
                    ++next;

                if (next < text.length() && (text[next] == '}' || text[next] == ']'))
                    continue;
            }

            result += c;
        }

        return result;
    }

    // VST3s carry moduleinfo.json on every platform; Audio Units only have Info.plist
    juce::String bundleVersion (const juce::File& bundle)
    {
        const auto moduleInfo = bundle.getChildFile ("Contents/Resources/moduleinfo.json");
        juce::var json;

        if (moduleInfo.existsAsFile()
            && juce::JSON::parse (withoutTrailingCommas (moduleInfo.loadFileAsString()), json).wasOk()
            && json["Version"].toString().isNotEmpty())
            return json["Version"].toString();

        // JUCE writes Info.plist as XML
        if (const auto plist = juce::parseXML (bundle.getChildFile ("Contents/Info.plist")))
            if (const auto* dict = plist->getChildByName ("dict"))
                for (const auto* key : dict->getChildWithTagNameIterator ("key"))
                    if (key->getAllSubText() == "CFBundleShortVersionString")
                        if (const auto* value = key->getNextElement())
                            return value->getAllSubText().trim();

        return {};
    }
}

PluginInstaller::PluginInstaller (Manifest m, juce::File payload, juce::File root)
    : manifest (std::move (m)), payloadDir (std::move (payload)), installRoot (std::move (root))
{
}

juce::File PluginInstaller::installedLocation (const Manifest::Bundle& bundle) const
{
    return platform::installFolder (bundle.format, installRoot).getChildFile (bundle.name);
}

PluginInstaller::Status PluginInstaller::status (const Manifest::Bundle& bundle) const
{
    using State = Status::State;
    const auto location = installedLocation (bundle);

    if (! location.isDirectory())
        return {};

    const auto installed = bundleVersion (location);
    if (installed.isEmpty())
        return { State::unknownVersion, {} };

    const auto order = compareVersions (installed, manifest.version);
    return { order < 0 ? State::older : order == 0 ? State::same : State::newer, installed };
}

juce::String PluginInstaller::Status::describe() const
{
    switch (state)
    {
        case State::notInstalled:   return "Not installed";
        case State::unknownVersion: return "Installed";
        case State::older:          return "Update from " + installedVersion;
        case State::same:           return "Already installed";
        case State::newer:          return "Newer version installed (" + installedVersion + ")";
    }

    return {};
}

InstallResult PluginInstaller::install (const std::vector<Manifest::Bundle>& bundles) const
{
    return platform::hasAdminRights() ? installHere (bundles) : runAsAdmin ("install", bundles, "install");
}

InstallResult PluginInstaller::uninstall (const std::vector<Manifest::Bundle>& bundles) const
{
    return platform::hasAdminRights() ? uninstallHere (bundles) : runAsAdmin ("uninstall", bundles, "remove");
}

// Hands the work to InstallerHelper, run with admin rights
InstallResult PluginInstaller::runAsAdmin (const juce::String& command, const std::vector<Manifest::Bundle>& bundles,
                                           const juce::String& action) const
{
    juce::StringArray args { command, payloadDir.getFullPathName(), installRoot.getFullPathName() };
    for (const auto& bundle : bundles)
        args.add (bundle.name);

    return platform::runHelperAsAdmin (args, "Enter your password to " + action + " " + manifest.product + ".");
}

InstallResult PluginInstaller::installHere (const std::vector<Manifest::Bundle>& bundles) const
{
    bool audioUnitsChanged = false;
    const juce::ScopeGuard refresh { [&] { refreshAudioUnits (audioUnitsChanged); } };

    for (const auto& bundle : bundles)
    {
        const auto source = payloadDir.getChildFile (bundle.name);
        const auto destination = installedLocation (bundle);
        const auto folder = destination.getParentDirectory();

        // Copy beside the old version first, so a failed copy leaves it untouched
        const auto staging = folder.getChildFile ("." + bundle.name + ".installing");

        if (! folder.createDirectory())
            return fail ("Couldn't create " + folder.getFullPathName());

        staging.deleteRecursively();

        if (! platform::copyBundle (source, staging))
        {
            staging.deleteRecursively();
            return fail ("Couldn't copy " + bundle.name + " to " + folder.getFullPathName());
        }

        if (destination.exists() && ! destination.deleteRecursively())
            return fail ("Couldn't replace " + destination.getFullPathName());

        if (! staging.moveFileTo (destination))
            return fail ("Couldn't move " + bundle.name + " into " + folder.getFullPathName());

        platform::afterInstall (destination);
        log ("Installed " + bundle.name + " in " + folder.getFullPathName());
        audioUnitsChanged = audioUnitsChanged || bundle.format == Manifest::Format::au;
    }

    return { InstallResult::Outcome::done, {} };
}

InstallResult PluginInstaller::uninstallHere (const std::vector<Manifest::Bundle>& bundles) const
{
    bool audioUnitsChanged = false;
    const juce::ScopeGuard refresh { [&] { refreshAudioUnits (audioUnitsChanged); } };

    for (const auto& bundle : bundles)
    {
        const auto location = installedLocation (bundle);

        if (! location.exists())
            continue;

        if (! location.deleteRecursively())
            return fail ("Couldn't remove " + location.getFullPathName());

        log ("Removed " + bundle.name + " from " + location.getParentDirectory().getFullPathName());
        audioUnitsChanged = audioUnitsChanged || bundle.format == Manifest::Format::au;
    }

    return { InstallResult::Outcome::done, {} };
}

// Runs on every exit from installHere/uninstallHere, since earlier bundles may have changed before a failure
void PluginInstaller::refreshAudioUnits (bool audioUnitsChanged) const
{
    platform::afterChanges (audioUnitsChanged);
    if (audioUnitsChanged)
        log ("Refreshed the Audio Unit cache");
}

InstallResult PluginInstaller::fail (const juce::String& reason) const
{
    log ("FAILED: " + reason);
    return { InstallResult::Outcome::failed, reason };
}

std::optional<std::vector<Manifest::Bundle>> PluginInstaller::findBundles (const juce::StringArray& names) const
{
    std::vector<Manifest::Bundle> found;

    for (const auto& name : names)
    {
        const auto match = std::find_if (manifest.bundles.begin(), manifest.bundles.end(),
                                         [&] (const auto& bundle) { return bundle.name == name; });
        if (match == manifest.bundles.end())
            return std::nullopt;

        found.push_back (*match);
    }

    return found;
}

juce::File PluginInstaller::logFile() const
{
    const auto vendor = juce::File::createLegalFileName (manifest.vendor);
    return platform::logFolder (vendor.isNotEmpty() ? vendor : "Plugins", installRoot).getChildFile ("Installer.log");
}

void PluginInstaller::log (const juce::String& line) const
{
    const auto file = logFile();
    file.getParentDirectory().createDirectory();
    file.appendText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S ") + line + "\n");
}

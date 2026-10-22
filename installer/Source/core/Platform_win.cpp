#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <RestartManager.h>

#include "ExePayload.h"
#include "HelperCommand.h"
#include "Platform.h"

#pragma comment (lib, "Rstrtmgr.lib")

namespace
{
    constexpr auto uninstallerName = "Uninstall.exe";

    // Not a test install root
    bool isRealInstall (const juce::File& installRoot)
    {
        return installRoot == platform::defaultInstallRoot();
    }

    // A test install root gets the same layout beneath it
    juce::File atRoot (juce::File::SpecialLocationType location, const juce::File& installRoot)
    {
        const auto real = juce::File::getSpecialLocation (location);

        if (isRealInstall (installRoot))
            return real;

        return installRoot.getChildFile (real.getFullPathName().fromFirstOccurrenceOf (":\\", false, false));
    }

    // For CommandLineToArgvW, which JUCE reads arguments with
    juce::String quoteArgument (const juce::String& arg)
    {
        juce::String result ("\"");
        int backslashes = 0;

        for (int i = 0; i < arg.length(); ++i)
        {
            const auto c = arg[i];

            if (c == '\\')
            {
                ++backslashes;
                continue;
            }

            result += juce::String::repeatedString ("\\", c == '"' ? backslashes * 2 + 1 : backslashes);
            result += c;
            backslashes = 0;
        }

        return result + juce::String::repeatedString ("\\", backslashes * 2) + "\"";
    }

    juce::String uninstallKey (const Manifest& manifest)
    {
        return "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + manifest.bundleId;
    }

    // e.g. C:\Program Files\Acme\Acme Reverb
    juce::File uninstallerFolder (const Manifest& manifest, const juce::File& installRoot)
    {
        return atRoot (juce::File::globalApplicationsDirectory, installRoot)
            .getChildFile (juce::File::createLegalFileName (manifest.vendor))
            .getChildFile (juce::File::createLegalFileName (manifest.product));
    }

    void deleteNowOrAtRestart (const juce::File& file)
    {
        if (file.exists() && ! file.deleteFile())
            MoveFileExW (file.getFullPathName().toWideCharPointer(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    // A running exe can be moved but not deleted, so move it aside
    void removeUninstaller (const juce::File& exe)
    {
        if (! exe.exists() || exe.deleteFile())
            return;

        const auto aside = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("Uninstall", ".exe", false);
        deleteNowOrAtRestart (exe.moveFileTo (aside) ? aside : exe);
    }

    // Keeps the window painting meanwhile, like the Mac password prompt
    void waitForProcess (HANDLE process)
    {
        while (MsgWaitForMultipleObjects (1, &process, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        {
            MSG message;
            while (PeekMessageW (&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage (&message);
                DispatchMessageW (&message);
            }
        }
    }
}

namespace platform
{
    juce::File defaultInstallRoot()
    {
        return juce::File (juce::SystemStats::getEnvironmentVariable ("SystemDrive", "C:") + "\\");
    }

    // VST3 only: Manifest::load rejects Audio Units here
    juce::File installFolder (Manifest::Format, const juce::File& installRoot)
    {
        return atRoot (juce::File::windowsProgramFilesCommon, installRoot).getChildFile ("VST3");
    }

    juce::File logFolder (const juce::String& vendor, const juce::File& installRoot)
    {
        return atRoot (juce::File::commonApplicationDataDirectory, installRoot).getChildFile (vendor).getChildFile ("Logs");
    }

    bool supportsFormat (Manifest::Format format)
    {
        return format == Manifest::Format::vst3;
    }

    bool hasAdminRights()
    {
        HANDLE token = nullptr;
        if (! OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;

        TOKEN_ELEVATION elevation {};
        DWORD size = 0;
        const auto read = GetTokenInformation (token, TokenElevation, &elevation, static_cast<DWORD> (sizeof (elevation)), &size);
        CloseHandle (token);
        return read && elevation.TokenIsElevated != 0;
    }

    juce::String adminPromptHint()
    {
        return "Click Yes when Windows asks to allow changes.";
    }

    // UAC names the exe itself, so the prompt text is unused
    InstallResult runHelperAsAdmin (const juce::StringArray& helperArgs, const juce::String&)
    {
        // The elevated run writes any error here
        const juce::TemporaryFile resultFile (".txt");

        juce::StringArray quoted { quoteArgument (commandLine::helper), quoteArgument (resultFile.getFile().getFullPathName()) };
        for (const auto& arg : helperArgs)
            quoted.add (quoteArgument (arg));

        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
        const auto parameters = quoted.joinIntoString (" ");

        SHELLEXECUTEINFOW info {};
        info.cbSize = static_cast<DWORD> (sizeof (info));
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        info.hwnd = GetActiveWindow();
        info.lpVerb = L"runas";
        info.lpFile = exe.toWideCharPointer();
        info.lpParameters = parameters.toWideCharPointer();
        info.nShow = SW_HIDE;

        if (! ShellExecuteExW (&info) || info.hProcess == nullptr)
            return GetLastError() == ERROR_CANCELLED
                 ? InstallResult { InstallResult::Outcome::cancelled, {} }
                 : InstallResult { InstallResult::Outcome::failed, "Couldn't get permission to make changes." };

        waitForProcess (info.hProcess);
        DWORD exitCode = 1;
        GetExitCodeProcess (info.hProcess, &exitCode);
        CloseHandle (info.hProcess);

        if (exitCode == 0)
            return { InstallResult::Outcome::done, {} };

        const auto error = resultFile.getFile().loadFileAsString().trim();
        return { InstallResult::Outcome::failed, error.isNotEmpty() ? error : juce::String ("The installer stopped unexpectedly.") };
    }

    // Asks Windows' Restart Manager
    juce::StringArray appsUsing (const juce::File& bundle)
    {
        juce::StringArray files;
        for (const auto& entry : juce::RangedDirectoryIterator (bundle, true, "*", juce::File::findFiles))
            files.add (entry.getFile().getFullPathName());

        if (files.isEmpty())
            return {};

        DWORD session = 0;
        WCHAR sessionKey[CCH_RM_SESSION_KEY + 1] {};
        if (RmStartSession (&session, 0, sessionKey) != ERROR_SUCCESS)
            return {};

        const juce::ScopeGuard endSession { [session] { RmEndSession (session); } };

        std::vector<LPCWSTR> paths;
        for (const auto& file : files)
            paths.push_back (file.toWideCharPointer());

        if (RmRegisterResources (session, static_cast<UINT> (paths.size()), paths.data(), 0, nullptr, 0, nullptr) != ERROR_SUCCESS)
            return {};

        std::vector<RM_PROCESS_INFO> processes;
        UINT needed = 0, count = 0;
        DWORD reasons = 0;
        auto status = RmGetList (session, &needed, &count, nullptr, &reasons);

        if (status == ERROR_MORE_DATA)
        {
            processes.resize (needed);
            count = needed;
            status = RmGetList (session, &needed, &count, processes.data(), &reasons);
        }

        juce::StringArray apps;
        if (status == ERROR_SUCCESS)
            for (UINT i = 0; i < count; ++i)
                apps.addIfNotAlreadyThere (juce::String (processes[i].strAppName));

        return apps;
    }

    // copyFileTo only copies files on Windows
    bool copyBundle (const juce::File& source, const juce::File& destination)
    {
        return source.isDirectory() ? source.copyDirectoryTo (destination) : source.copyFileTo (destination);
    }

    // Removes the "downloaded from the internet" mark
    void afterInstall (const juce::File& bundle)
    {
        for (const auto& entry : juce::RangedDirectoryIterator (bundle, true, "*", juce::File::findFiles))
            DeleteFileW ((entry.getFile().getFullPathName() + ":Zone.Identifier").toWideCharPointer());
    }

    void afterChanges (bool) {}

    bool registerProduct (const Manifest& manifest, const juce::File& payloadDir, const juce::File& installRoot)
    {
        const auto folder = uninstallerFolder (manifest, installRoot);
        const auto uninstaller = folder.getChildFile (uninstallerName);
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

        if (! folder.createDirectory() || ! exePayload::copyWithoutPayload (exe, uninstaller)
            || ! payloadDir.getChildFile (Manifest::fileName).copyFileTo (folder.getChildFile (Manifest::fileName)))
            return false;

        if (! isRealInstall (installRoot))
            return true;

        juce::int64 bytes = 0;
        for (const auto& bundle : manifest.bundles)
            for (const auto& entry : juce::RangedDirectoryIterator (installFolder (bundle.format, installRoot).getChildFile (bundle.name),
                                                                    true, "*", juce::File::findFiles))
                bytes += entry.getFileSize();

        const auto key = uninstallKey (manifest) + "\\";
        const auto set = [&key] (const juce::String& name, const juce::String& value)
        {
            return value.isEmpty() || juce::WindowsRegistry::setValue (key + name, value);
        };

        return set ("DisplayName", manifest.product)
            && set ("DisplayVersion", manifest.version)
            && set ("Publisher", manifest.vendor)
            && set ("URLInfoAbout", manifest.url)
            && set ("HelpLink", manifest.email.isNotEmpty() ? "mailto:" + manifest.email : juce::String())
            && set ("DisplayIcon", uninstaller.getFullPathName())
            && set ("InstallLocation", folder.getFullPathName())
            && set ("UninstallString", uninstaller.getFullPathName().quoted() + " " + commandLine::uninstall)
            && juce::WindowsRegistry::setValue (key + "NoModify", static_cast<juce::uint32> (1))
            && juce::WindowsRegistry::setValue (key + "NoRepair", static_cast<juce::uint32> (1))
            && juce::WindowsRegistry::setValue (key + "EstimatedSize", static_cast<juce::uint32> (bytes / 1024));
    }

    void unregisterProduct (const Manifest& manifest, const juce::File& installRoot)
    {
        if (isRealInstall (installRoot))
            juce::WindowsRegistry::deleteKey (uninstallKey (manifest));

        const auto folder = uninstallerFolder (manifest, installRoot);
        deleteNowOrAtRestart (folder.getChildFile (Manifest::fileName));
        removeUninstaller (folder.getChildFile (uninstallerName));
        deleteNowOrAtRestart (folder);

        // The vendor folder, once empty
        if (folder.getParentDirectory().getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
            folder.getParentDirectory().deleteFile();
    }
}

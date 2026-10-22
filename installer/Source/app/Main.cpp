#include <juce_gui_basics/juce_gui_basics.h>

#include "BrandLookAndFeel.h"
#include "HelperCommand.h"
#include "InstallerComponent.h"
#include "Payload.h"
#include "Platform.h"
#include "TitleBar.h"

#if JUCE_MAC
 #include "DmgBackground.h"
#endif

namespace
{
    bool isHelperRun (const juce::StringArray& args)
    {
        return args.size() >= 2 && args[0] == commandLine::helper;
    }

    // Packaging: --dmg-background <folder> <label> <layout.json>, run by dmg/build-dmg.sh
    bool isDmgBackgroundRun (const juce::StringArray& args)
    {
        return args.size() >= 4 && args[0] == "--dmg-background";
    }

    // Dev flags: --payload <dir>, --install-root <dir>; fallback is lazy
    juce::File fileArgument (const juce::StringArray& args, const juce::String& flag,
                             const std::function<juce::File()>& fallback)
    {
        const auto index = args.indexOf (flag);

        if (index < 0 || index + 1 >= args.size())
            return fallback();

        return juce::File::getCurrentWorkingDirectory().getChildFile (args[index + 1]);
    }
}

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, juce::Component* content, bool darkTitleBar)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());

        // After setResizable, which rebuilds the native window
        if (auto* peer = getPeer(); peer != nullptr && darkTitleBar)
            useDarkTitleBar (*peer);

        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class InstallerApplication final : public juce::JUCEApplication
{
public:
    // The app's file name, e.g. "Install Acme Reverb", since packaging renames it per product
    const juce::String getApplicationName() override
    {
        const auto name = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getFileNameWithoutExtension();
        return name.isNotEmpty() ? name : juce::String (JUCE_APPLICATION_NAME_STRING);
    }

    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    // JUCE's lock is keyed on the name above, so different products' installers can still run together
    // The helper run, Uninstall.exe and packaging skip it
    bool moreThanOneInstanceAllowed() override
    {
        const auto args = getCommandLineParameterArray();
        return isHelperRun (args) || args.contains (commandLine::uninstall) || isDmgBackgroundRun (args);
    }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();

        if (isHelperRun (args))
        {
            const auto result = runHelperCommand (juce::StringArray (args.begin() + 2, args.size() - 2));
            juce::File (args[1]).replaceWithText (result.message);
            setApplicationReturnValue (result.outcome == InstallResult::Outcome::done ? 0 : 1);
            quit();
            return;
        }

        theme = std::make_unique<Theme> (Theme::load());
        lookAndFeel = std::make_unique<BrandLookAndFeel> (*theme);
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

       #if JUCE_MAC
        if (isDmgBackgroundRun (args))
        {
            const auto here = juce::File::getCurrentWorkingDirectory();
            const auto written = writeDmgBackground (*theme, args[2], here.getChildFile (args[3]), here.getChildFile (args[1]));
            setApplicationReturnValue (written ? 0 : 1);
            quit();
            return;
        }
       #endif

        // From Apps & Features; installer.json sits beside Uninstall.exe
        const auto uninstalling = args.contains (commandLine::uninstall);
        const auto exeFolder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const auto payloadDir = fileArgument (args, "--payload", [&] { return uninstalling ? exeFolder : defaultPayloadFolder(); });
        const auto installRoot = fileArgument (args, "--install-root", platform::defaultInstallRoot);

        Manifest manifest;
        auto windowTitle = getApplicationName();
        std::unique_ptr<juce::Component> content;

        if (const auto result = Manifest::load (payloadDir, manifest, ! uninstalling); result.wasOk())
        {
            windowTitle = (uninstalling ? "Remove " : "Install ") + manifest.product;
            content = std::make_unique<InstallerComponent> (*theme,
                std::make_unique<PluginInstaller> (std::move (manifest), payloadDir, installRoot), uninstalling);
        }
        else
        {
            content = std::make_unique<InstallerComponent> (*theme, result.getErrorMessage());
        }

        mainWindow = std::make_unique<MainWindow> (windowTitle, content.release(), theme->isDark());
    }

    void shutdown() override
    {
        // However the window was closed; not after a helper or packaging run
        if (mainWindow != nullptr)
            releasePayload();

        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel = nullptr;
        theme = nullptr;
    }

    void systemRequestedQuit() override
    {
        // Quitting mid-install would tear the app down inside the password prompt
        if (auto* content = mainWindow != nullptr ? dynamic_cast<InstallerComponent*> (mainWindow->getContentComponent()) : nullptr)
            if (content->isBusy())
                return;

        quit();
    }

private:
    std::unique_ptr<Theme> theme;
    std::unique_ptr<BrandLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (InstallerApplication)

#include <juce_gui_basics/juce_gui_basics.h>

#include "InstallerComponent.h"
#include "Payload.h"
#include "Platform.h"

namespace
{
    // Development flags: --payload <dir> and --install-root <dir>
    juce::File fileArgument (const juce::StringArray& args, const juce::String& flag, const juce::File& fallback)
    {
        const auto index = args.indexOf (flag);

        if (index < 0 || index + 1 >= args.size())
            return fallback;

        return juce::File::getCurrentWorkingDirectory().getChildFile (args[index + 1]);
    }
}

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (const juce::String& name, juce::Component* content)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
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
    // The app's file name, e.g. "Install SampleRealm Reverb", since packaging renames it per product
    const juce::String getApplicationName() override
    {
        const auto name = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getFileNameWithoutExtension();
        return name.isNotEmpty() ? name : juce::String (JUCE_APPLICATION_NAME_STRING);
    }

    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    // JUCE's lock is keyed on the name above, so different products' installers can still run together
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();
        const auto payloadDir = fileArgument (args, "--payload", defaultPayloadFolder());
        const auto installRoot = fileArgument (args, "--install-root", platform::defaultInstallRoot());

        Manifest manifest;
        auto windowTitle = getApplicationName();
        std::unique_ptr<juce::Component> content;

        if (const auto result = Manifest::load (payloadDir, manifest); result.wasOk())
        {
            windowTitle = "Install " + manifest.product;
            content = std::make_unique<InstallerComponent> (
                std::make_unique<PluginInstaller> (std::move (manifest), payloadDir, installRoot));
        }
        else
        {
            content = std::make_unique<InstallerComponent> (result.getErrorMessage());
        }

        mainWindow = std::make_unique<MainWindow> (windowTitle, content.release());
    }

    void shutdown() override { mainWindow = nullptr; }

    void systemRequestedQuit() override
    {
        // Quitting mid-install would tear the app down inside the password prompt
        if (auto* content = mainWindow != nullptr ? dynamic_cast<InstallerComponent*> (mainWindow->getContentComponent()) : nullptr)
            if (content->isBusy())
                return;

        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (InstallerApplication)

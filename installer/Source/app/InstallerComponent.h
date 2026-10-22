#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FormatRow.h"
#include "PluginInstaller.h"

// Welcome → installing or removing → finished or failed, or one error screen if there's nothing to install
class InstallerComponent final : public juce::Component
{
public:
    // removeOnly: the uninstaller's screen
    InstallerComponent (const Theme&, std::unique_ptr<PluginInstaller>, bool removeOnly);
    InstallerComponent (const Theme&, const juce::String& error);

    void paint (juce::Graphics&) override;
    void resized() override;

    // True while installing or removing; the app mustn't quit then.
    bool isBusy() const { return busy; }

private:
    struct Row
    {
        Row (const Theme& brand, const Manifest::Bundle& b) : bundle (b), button (brand, formatDisplayName (b.format)) {}

        Manifest::Bundle bundle;
        PluginInstaller::Status installStatus;
        FormatRow button;
    };

    enum class Operation { install, remove };

    void styleLabels();
    void addRow (const Manifest::Bundle&);
    void refreshRow (Row&);
    void updateButtons();
    std::vector<Manifest::Bundle> selectedBundles (bool installedOnly) const;

    void showWelcome();
    void confirmInstall();
    void confirmRemove();
    void confirm (const juce::String& question, const juce::String& detail, const juce::String& button,
                  std::function<void()> onConfirm);
    void start (Operation);
    void showFinished (Operation, const std::vector<Manifest::Bundle>&);
    void showFailed (Operation, const juce::String& reason);
    void showOnly (const std::vector<juce::Component*>&);

    const Theme& theme;
    std::unique_ptr<PluginInstaller> installer;
    const bool removeOnly = false;
    std::vector<std::unique_ptr<Row>> rows;
    bool busy = false;

    // Header labels, then the current step's status and message
    juce::Label category, title, subtitle, status, message, footer;
    juce::HyperlinkButton websiteLink;
    juce::TextButton cancelButton, actionButton, removeButton { "Remove" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerComponent)
};

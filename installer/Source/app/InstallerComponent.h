#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginInstaller.h"

// Welcome → installing or removing → finished or failed, or one error screen if there's nothing to install
class InstallerComponent final : public juce::Component
{
public:
    explicit InstallerComponent (std::unique_ptr<PluginInstaller>);
    explicit InstallerComponent (const juce::String& error);

    void resized() override;

    // True while installing or removing; the app mustn't quit then.
    bool isBusy() const { return busy; }

private:
    struct Row
    {
        Manifest::Bundle bundle;
        PluginInstaller::Status installStatus;
        juce::ToggleButton toggle;
        juce::Label status;
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
    void showOnly (std::initializer_list<juce::Component*>);

    std::unique_ptr<PluginInstaller> installer;
    std::vector<std::unique_ptr<Row>> rows;
    bool busy = false;

    juce::Label title, subtitle, message, footer;
    juce::HyperlinkButton websiteLink;
    juce::TextButton cancelButton, actionButton, removeButton { "Remove" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstallerComponent)
};

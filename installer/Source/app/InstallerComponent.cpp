#include "InstallerComponent.h"
#include "Payload.h"
#include "Platform.h"

namespace
{
    const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    const juce::String tick (juce::CharPointer_UTF8 ("\xe2\x9c\x93"));

    juce::String categoryText (const juce::String& category)
    {
        if (category == "Fx")         return "Audio effect";
        return category;
    }

    void styleLabel (juce::Label& label, float height, bool bold = false)
    {
        label.setFont (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    void quit() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
}

InstallerComponent::InstallerComponent (std::unique_ptr<PluginInstaller> pluginInstaller, bool removeOnlyScreen)
    : installer (std::move (pluginInstaller)), removeOnly (removeOnlyScreen)
{
    const auto& manifest = installer->getManifest();

    styleLabels();

    juce::StringArray details { "Version " + manifest.version };
    if (manifest.category.isNotEmpty())
        details.add (categoryText (manifest.category));
    subtitle.setText (details.joinIntoString (dot), juce::dontSendNotification);

    if (manifest.url.isNotEmpty())
    {
        websiteLink.setButtonText (juce::URL (manifest.url).getDomain());
        websiteLink.setURL (juce::URL (manifest.url));
        websiteLink.setJustificationType (juce::Justification::centredLeft);
    }

    for (const auto& bundle : manifest.bundles)
        addRow (bundle);

    for (auto* c : std::initializer_list<juce::Component*> { &title, &subtitle, &message, &footer, &websiteLink,
                                                             &cancelButton, &actionButton, &removeButton })
        addChildComponent (c);

    cancelButton.onClick = quit;
    removeButton.onClick = [this] { confirmRemove(); };
    showWelcome();
    setSize (520, 360);
}

InstallerComponent::InstallerComponent (const juce::String& error)
{
    styleLabels();

    title.setText ("Can't install", juce::dontSendNotification);
    message.setText (error, juce::dontSendNotification);

    cancelButton.setButtonText ("Quit");
    cancelButton.onClick = quit;

    for (auto* c : std::initializer_list<juce::Component*> { &title, &message, &cancelButton })
        addAndMakeVisible (c);

    setSize (520, 360);
}

void InstallerComponent::styleLabels()
{
    styleLabel (title, 26.0f, true);
    styleLabel (subtitle, 15.0f);
    styleLabel (message, 15.0f);
    styleLabel (footer, 12.0f);
}

void InstallerComponent::addRow (const Manifest::Bundle& bundle)
{
    auto row = std::make_unique<Row>();
    row->bundle = bundle;

    row->toggle.setButtonText (formatDisplayName (bundle.format));
    row->toggle.setToggleState (true, juce::dontSendNotification);
    row->toggle.onClick = [this] { updateButtons(); };

    row->status.setJustificationType (juce::Justification::centredRight);
    row->status.setColour (juce::Label::textColourId,
                           getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.7f));

    addChildComponent (row->toggle);
    addChildComponent (row->status);
    rows.push_back (std::move (row));
}

void InstallerComponent::refreshRow (Row& row)
{
    row.installStatus = installer->status (row.bundle);
    row.status.setText (row.installStatus.describe(), juce::dontSendNotification);
}

void InstallerComponent::updateButtons()
{
    if (removeOnly)
    {
        actionButton.setButtonText ("Remove");
        actionButton.setEnabled (true);
        return;
    }

    using State = PluginInstaller::Status::State;
    int selected = 0, updates = 0, reinstalls = 0;

    for (const auto& row : rows)
    {
        if (! row->toggle.getToggleState())
            continue;

        ++selected;
        updates += row->installStatus.state == State::older ? 1 : 0;
        reinstalls += row->installStatus.state == State::same ? 1 : 0;
    }

    actionButton.setButtonText (selected > 0 && updates == selected      ? "Update"
                                : selected > 0 && reinstalls == selected ? "Reinstall"
                                                                         : "Install");
    actionButton.setEnabled (selected > 0);

    // Remove acts on the ticked formats that are installed
    removeButton.setEnabled (! selectedBundles (true).empty());
}

std::vector<Manifest::Bundle> InstallerComponent::selectedBundles (bool installedOnly) const
{
    std::vector<Manifest::Bundle> bundles;

    for (const auto& row : rows)
        if (row->toggle.getToggleState()
            && (! installedOnly || row->installStatus.state != PluginInstaller::Status::State::notInstalled))
            bundles.push_back (row->bundle);

    return bundles;
}

void InstallerComponent::showOnly (std::initializer_list<juce::Component*> visible)
{
    for (auto* child : getChildren())
        child->setVisible (std::find (visible.begin(), visible.end(), child) != visible.end());
}

void InstallerComponent::showWelcome()
{
    const auto& manifest = installer->getManifest();

    title.setText (removeOnly ? "Remove " + manifest.product : manifest.product, juce::dontSendNotification);
    footer.setText (manifest.copyright, juce::dontSendNotification);

    cancelButton.setButtonText ("Cancel");

    // The uninstaller's screen is itself the confirmation
    actionButton.onClick = [this] { removeOnly ? start (Operation::remove) : confirmInstall(); };

    bool anyInstalled = false;
    for (const auto& row : rows)
    {
        refreshRow (*row);
        anyInstalled = anyInstalled || row->installStatus.state != PluginInstaller::Status::State::notInstalled;
    }
    updateButtons();

    showOnly ({ &title, &subtitle, &footer, &cancelButton, &actionButton });
    removeButton.setVisible (anyInstalled && ! removeOnly);
    for (const auto& row : rows)
    {
        row->toggle.setVisible (true);
        row->toggle.setEnabled (! removeOnly);
        row->status.setVisible (true);
    }
}

void InstallerComponent::confirmInstall()
{
    const auto& manifest = installer->getManifest();
    juce::String newerVersion;

    for (const auto& row : rows)
        if (row->toggle.getToggleState() && row->installStatus.state == PluginInstaller::Status::State::newer)
            newerVersion = row->installStatus.installedVersion;

    if (newerVersion.isEmpty())
    {
        start (Operation::install);
        return;
    }

    confirm ("Replace a newer version?",
             manifest.product + " " + newerVersion + " is installed. Replace it with " + manifest.version + "?",
             "Replace", [this] { start (Operation::install); });
}

void InstallerComponent::confirmRemove()
{
    juce::StringArray formats;
    for (const auto& bundle : selectedBundles (true))
        formats.add (formatDisplayName (bundle.format));

    confirm ("Remove " + installer->getManifest().product + "?",
             "This removes the " + formats.joinIntoString (" and ") + (formats.size() > 1 ? " versions" : " version")
                 + " from this computer.",
             "Remove", [this] { start (Operation::remove); });
}

void InstallerComponent::confirm (const juce::String& question, const juce::String& detail, const juce::String& button,
                                  std::function<void()> onConfirm)
{
    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon,
                                                                        question, detail, button, "Cancel", this);

    juce::NativeMessageBox::showAsync (options, [safe = juce::Component::SafePointer (this), onConfirm] (int result)
    {
        // NativeMessageBox::showAsync reports the clicked button's position: ours is first, so 0
        if (safe != nullptr && result == 0)
            onConfirm();
    });
}

void InstallerComponent::start (Operation operation)
{
    if (busy)
        return;

    busy = true;
    const auto installing = operation == Operation::install;
    // Everything, so the uninstaller always clears its Apps & Features entry
    const auto bundles = removeOnly ? installer->getManifest().bundles : selectedBundles (! installing);
    const auto finished = removeOnly ? selectedBundles (true) : bundles;

    title.setText (installing ? "Installing..." : "Removing...", juce::dontSendNotification);
    message.setText (platform::adminPromptHint(), juce::dontSendNotification);
    showOnly ({ &title, &subtitle, &message });

    // Not a timer: the password prompt runs the event loop while it waits, so a timer would fire again
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer (this), operation, installing, bundles, finished]
    {
        if (safe == nullptr)
            return;

        const auto result = installing ? safe->installer->install (bundles) : safe->installer->uninstall (bundles);
        safe->busy = false;

        switch (result.outcome)
        {
            case InstallResult::Outcome::done:      safe->showFinished (operation, finished); break;
            case InstallResult::Outcome::cancelled: safe->showWelcome(); break;
            case InstallResult::Outcome::failed:    safe->showFailed (operation, result.message); break;
        }
    });
}

void InstallerComponent::showFinished (Operation operation, const std::vector<Manifest::Bundle>& bundles)
{
    const auto& manifest = installer->getManifest();
    const auto installing = operation == Operation::install;

    juce::StringArray lines;
    for (const auto& bundle : bundles)
        lines.add (tick + "  " + formatDisplayName (bundle.format) + (installing ? " installed in " : " removed from ")
                   + installer->installedLocation (bundle).getParentDirectory().getFullPathName());

    if (bundles.empty())
    {
        lines.add (manifest.product + " was already removed.");
    }
    else
    {
        lines.add ({});
        lines.add (installing ? "Restart your DAW to use " + manifest.product + "."
                              : "Restart your DAW so it stops listing " + manifest.product + ".");
    }

    title.setText (installing ? "Installed" : "Removed", juce::dontSendNotification);
    message.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);

    actionButton.setButtonText ("Done");
    actionButton.setEnabled (true);
    actionButton.onClick = [] { releasePayload(); quit(); };

    showOnly ({ &title, &subtitle, &message, &actionButton });
    websiteLink.setVisible (installing && manifest.url.isNotEmpty());
}

void InstallerComponent::showFailed (Operation operation, const juce::String& reason)
{
    const auto& manifest = installer->getManifest();

    title.setText (operation == Operation::install ? "Installation failed" : "Couldn't remove",
                   juce::dontSendNotification);
    message.setText (reason + "\n\nDetails: " + installer->logFile().getFullPathName(), juce::dontSendNotification);
    footer.setText (manifest.email.isNotEmpty() ? "Need help? Contact " + manifest.email : juce::String(),
                    juce::dontSendNotification);

    cancelButton.setButtonText ("Quit");
    actionButton.setButtonText ("Try Again");
    actionButton.setEnabled (true);
    actionButton.onClick = [this] { showWelcome(); };

    showOnly ({ &title, &subtitle, &message, &footer, &cancelButton, &actionButton });
}

void InstallerComponent::resized()
{
    auto area = getLocalBounds().reduced (28);

    auto buttons = area.removeFromBottom (32);
    actionButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (10);
    cancelButton.setBounds (buttons.removeFromRight (110));

    // Never shown together: Remove on the welcome screen, the link once installed
    removeButton.setBounds (buttons.withWidth (110));
    websiteLink.setBounds (buttons);

    area.removeFromBottom (12);
    footer.setBounds (area.removeFromBottom (18));

    title.setBounds (area.removeFromTop (36));
    subtitle.setBounds (area.removeFromTop (24));
    area.removeFromTop (20);

    message.setBounds (area);

    for (const auto& row : rows)
    {
        auto line = area.removeFromTop (30);
        row->toggle.setBounds (line.removeFromLeft (180));
        row->status.setBounds (line);
    }
}

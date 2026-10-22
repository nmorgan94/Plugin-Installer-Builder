#include "InstallerComponent.h"
#include "Platform.h"

namespace
{
    constexpr int panelWidth = 180;

    juce::String categoryText (const juce::String& category)
    {
        if (category == "Fx")         return "Audio effect";
        return category;
    }

    void styleLabel (juce::Label& label, const juce::Font& font)
    {
        label.setFont (font);
        label.setBorderSize ({});
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    void quit() { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
}

InstallerComponent::InstallerComponent (const Theme& brand, std::unique_ptr<PluginInstaller> pluginInstaller,
                                        bool removeOnlyScreen)
    : theme (brand), installer (std::move (pluginInstaller)), removeOnly (removeOnlyScreen)
{
    const auto& manifest = installer->getManifest();

    styleLabels();

    category.setText (categoryText (manifest.category).toUpperCase(), juce::dontSendNotification);
    title.setText (manifest.name, juce::dontSendNotification);
    subtitle.setText ("Version " + manifest.version, juce::dontSendNotification);

    if (manifest.url.isNotEmpty())
    {
        websiteLink.setButtonText (juce::URL (manifest.url).getDomain());
        websiteLink.setURL (juce::URL (manifest.url));
        websiteLink.setFont (theme.bodyFont (14.0f), false, juce::Justification::centredLeft);
    }

    for (const auto& bundle : manifest.bundles)
        addRow (bundle);

    for (auto* c : std::initializer_list<juce::Component*> { &category, &title, &subtitle, &status, &message, &footer,
                                                             &websiteLink, &cancelButton, &actionButton, &removeButton })
        addChildComponent (c);

    cancelButton.onClick = quit;
    removeButton.onClick = [this] { confirmRemove(); };
    setSize (560, 400);
    showWelcome();
}

InstallerComponent::InstallerComponent (const Theme& brand, const juce::String& error)
    : theme (brand)
{
    styleLabels();

    title.setText ("Can't install", juce::dontSendNotification);
    message.setText (error, juce::dontSendNotification);

    actionButton.setButtonText ("Quit");
    actionButton.onClick = quit;

    for (auto* c : std::initializer_list<juce::Component*> { &title, &message, &actionButton })
        addAndMakeVisible (c);

    setSize (560, 400);
}

void InstallerComponent::styleLabels()
{
    styleLabel (category, theme.bodyFont (12.0f, true).withExtraKerningFactor (0.08f));
    styleLabel (title, theme.headingFont (28.0f));
    styleLabel (subtitle, theme.bodyFont (14.0f));
    styleLabel (status, theme.bodyFont (15.0f, true));
    styleLabel (message, theme.bodyFont (14.0f));
    styleLabel (footer, theme.bodyFont (12.0f));

    category.setColour (juce::Label::textColourId, theme.accentText);
    for (auto* label : { &subtitle, &message, &footer })
        label->setColour (juce::Label::textColourId, theme.mutedText);

    // Text only; the action button is the one filled in
    const auto textOnly = [] (juce::TextButton& button, juce::Colour colour)
    {
        button.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button.setColour (juce::TextButton::textColourOffId, colour);
    };

    textOnly (cancelButton, theme.text);
    textOnly (removeButton, theme.mutedText);
}

void InstallerComponent::addRow (const Manifest::Bundle& bundle)
{
    auto row = std::make_unique<Row> (theme, bundle);
    row->button.setToggleState (true, juce::dontSendNotification);
    row->button.onClick = [this] { updateButtons(); };

    addChildComponent (row->button);
    rows.push_back (std::move (row));
}

void InstallerComponent::refreshRow (Row& row)
{
    using State = PluginInstaller::Status::State;

    row.installStatus = installer->status (row.bundle);
    const auto state = row.installStatus.state;
    row.button.setDetail (row.installStatus.describe(),
                          state == State::older || state == State::newer ? theme.highlight : theme.mutedText);
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
        if (! row->button.getToggleState())
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
        if (row->button.getToggleState()
            && (! installedOnly || row->installStatus.state != PluginInstaller::Status::State::notInstalled))
            bundles.push_back (row->bundle);

    return bundles;
}

// Layout depends on what's visible, so this also lays out and repaints
void InstallerComponent::showOnly (const std::vector<juce::Component*>& visible)
{
    for (auto* child : getChildren())
        child->setVisible (std::find (visible.begin(), visible.end(), child) != visible.end());

    resized();
    repaint();
}

void InstallerComponent::showWelcome()
{
    status.setText ("Remove from this computer", juce::dontSendNotification);
    footer.setText (installer->getManifest().copyright, juce::dontSendNotification);

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

    std::vector<juce::Component*> visible { &category, &title, &subtitle, &footer, &cancelButton, &actionButton };

    if (removeOnly)
        visible.push_back (&status);
    else if (anyInstalled)
        visible.push_back (&removeButton);

    for (const auto& row : rows)
    {
        row->button.setMark (removeOnly ? FormatRow::Mark::none : FormatRow::Mark::tickBox);
        visible.push_back (&row->button);
    }

    showOnly (visible);
}

void InstallerComponent::confirmInstall()
{
    const auto& manifest = installer->getManifest();
    juce::String newerVersion;

    for (const auto& row : rows)
        if (row->button.getToggleState() && row->installStatus.state == PluginInstaller::Status::State::newer)
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

    status.setText (installing ? "Installing..." : "Removing...", juce::dontSendNotification);
    message.setText (platform::adminPromptHint(), juce::dontSendNotification);
    showOnly ({ &category, &title, &subtitle, &status, &message });

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

    status.setText (installing ? "Installed" : "Removed", juce::dontSendNotification);
    message.setText (bundles.empty() ? manifest.product + " was already removed."
                     : installing    ? "Restart your DAW to use " + manifest.product + "."
                                     : "Restart your DAW so it stops listing " + manifest.product + ".",
                     juce::dontSendNotification);

    actionButton.setButtonText ("Done");
    actionButton.setEnabled (true);
    actionButton.onClick = quit;

    std::vector<juce::Component*> visible { &category, &title, &subtitle, &status, &message, &actionButton };

    if (installing && manifest.url.isNotEmpty())
        visible.push_back (&websiteLink);

    // A row per format changed, with the folder it's in or was in
    for (const auto& row : rows)
    {
        row->button.setMark (FormatRow::Mark::done);
        row->button.setDetail (installer->installedLocation (row->bundle).getParentDirectory().getFullPathName(),
                               theme.mutedText);

        if (std::any_of (bundles.begin(), bundles.end(), [&] (const auto& b) { return b.name == row->bundle.name; }))
            visible.push_back (&row->button);
    }

    showOnly (visible);
}

void InstallerComponent::showFailed (Operation operation, const juce::String& reason)
{
    const auto& manifest = installer->getManifest();

    status.setText (operation == Operation::install ? "Installation failed" : "Couldn't remove",
                    juce::dontSendNotification);
    message.setText (reason + "\n\nDetails: " + installer->logFile().getFullPathName(), juce::dontSendNotification);
    footer.setText (manifest.email.isNotEmpty() ? "Need help? Contact " + manifest.email : juce::String(),
                    juce::dontSendNotification);

    cancelButton.setButtonText ("Quit");
    actionButton.setButtonText ("Try Again");
    actionButton.setEnabled (true);
    actionButton.onClick = [this] { showWelcome(); };

    showOnly ({ &category, &title, &subtitle, &status, &message, &footer, &cancelButton, &actionButton });
}

void InstallerComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme.background);

    const auto panel = getLocalBounds().removeFromLeft (panelWidth).toFloat();
    g.setColour (theme.panel);
    g.fillRect (panel);

    // Logo above the vendor's name, centred together
    const auto logoSize = theme.logo != nullptr ? 96.0f : 0.0f;
    auto block = panel.withSizeKeepingCentre (panel.getWidth() - 24.0f, logoSize + (logoSize > 0.0f ? 40.0f : 24.0f));

    if (theme.logo != nullptr)
    {
        theme.logo->drawWithin (g, block.removeFromTop (logoSize), juce::RectanglePlacement::centred, 1.0f);
        block.removeFromTop (16.0f);
    }

    g.setColour (theme.text);
    g.setFont (theme.headingFont (17.0f));
    g.drawFittedText (installer != nullptr ? installer->getManifest().vendor : theme.company,
                      block.toNearestInt(), juce::Justification::centred, 1);

    // A line above each format row and below the last
    g.setColour (theme.line);
    juce::Rectangle<int> last;

    for (const auto& row : rows)
    {
        if (row->button.isVisible())
        {
            last = row->button.getBounds();
            g.fillRect (last.removeFromTop (1));
        }
    }

    if (! last.isEmpty())
        g.fillRect (last.removeFromBottom (1));
}

void InstallerComponent::resized()
{
    auto area = getLocalBounds().withTrimmedLeft (panelWidth).reduced (28, 0).withTrimmedTop (28).withTrimmedBottom (26);

    auto buttons = area.removeFromBottom (40);
    actionButton.setBounds (buttons.removeFromRight (120));
    buttons.removeFromRight (4);
    cancelButton.setBounds (buttons.removeFromRight (80));

    // Never shown together: Remove on the welcome screen, the link once installed
    removeButton.setBounds (buttons.withWidth (70));
    websiteLink.setBounds (buttons);

    area.removeFromBottom (12);
    footer.setBounds (area.removeFromBottom (16));

    category.setBounds (area.removeFromTop (16));
    area.removeFromTop (6);
    title.setBounds (area.removeFromTop (34));
    subtitle.setBounds (area.removeFromTop (20));
    area.removeFromTop (16);

    // Below the header, only what the current step shows
    if (status.isVisible())
    {
        status.setBounds (area.removeFromTop (22));
        area.removeFromTop (8);
    }

    bool anyRows = false;
    for (const auto& row : rows)
    {
        if (row->button.isVisible())
        {
            row->button.setBounds (area.removeFromTop (56));
            anyRows = true;
        }
    }

    if (anyRows)
        area.removeFromTop (12);

    message.setBounds (area);
}

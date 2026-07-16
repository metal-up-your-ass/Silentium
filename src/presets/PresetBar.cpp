#include "PresetBar.h"

namespace basilica::presets
{
    namespace
    {
        constexpr int arrowButtonWidth = 24;
        constexpr int actionButtonWidth = 84;
        constexpr int margin = 4;
        constexpr int timerIntervalMs = 250; // cheap polling for the dirty '*' indicator - see PresetBar.h
    }

    PresetBar::PresetBar (PresetManager& managerToControl) : manager (managerToControl)
    {
        previousButton.onClick = [this] { manager.previousPreset(); refreshFromManager(); };
        addAndMakeVisible (previousButton);

        nameButton.onClick = [this] { showPresetMenu(); };
        addAndMakeVisible (nameButton);

        nextButton.onClick = [this] { manager.nextPreset(); refreshFromManager(); };
        addAndMakeVisible (nextButton);

        saveButton.setButtonText (TRANS ("Save"));
        saveButton.onClick = [this] { manager.saveCurrentUserPreset(); refreshFromManager(); };
        addAndMakeVisible (saveButton);

        saveAsButton.setButtonText (TRANS ("Save As..."));
        saveAsButton.onClick = [this] { promptAndSaveAs(); };
        addAndMakeVisible (saveAsButton);

        deleteButton.setButtonText (TRANS ("Delete"));
        deleteButton.onClick = [this]
        {
            manager.deleteUserPreset (manager.getCurrentPresetName());
            refreshFromManager();
        };
        addAndMakeVisible (deleteButton);

        importButton.setButtonText (TRANS ("Import..."));
        importButton.onClick = [this] { promptAndImport(); };
        addAndMakeVisible (importButton);

        exportButton.setButtonText (TRANS ("Export..."));
        exportButton.onClick = [this] { promptAndExport(); };
        addAndMakeVisible (exportButton);

        refreshFromManager();
        startTimer (timerIntervalMs);
    }

    PresetBar::~PresetBar()
    {
        stopTimer();
    }

    void PresetBar::timerCallback()
    {
        refreshFromManager();
    }

    void PresetBar::refreshFromManager()
    {
        auto name = manager.getCurrentPresetName();

        if (name.isEmpty())
            name = TRANS ("Init");

        nameButton.setButtonText (manager.isDirty() ? name + "*" : name);

        const auto isUserPreset = ! manager.isCurrentPresetFactory() && manager.getCurrentPresetName().isNotEmpty();
        saveButton.setEnabled (isUserPreset && manager.isDirty());
        deleteButton.setEnabled (isUserPreset);
        exportButton.setEnabled (manager.getCurrentPresetName().isNotEmpty());
    }

    void PresetBar::showPresetMenu()
    {
        const auto allPresets = manager.getAllPresets();

        juce::PopupMenu factoryMenu;
        juce::PopupMenu userMenu;

        // PopupMenu item IDs are 1-based (0 means "nothing selected"); index
        // presetNames by (id - 1) in the async callback below.
        std::vector<juce::String> presetNames;

        for (auto& entry : allPresets)
        {
            presetNames.push_back (entry.name);
            const auto itemId = static_cast<int> (presetNames.size());
            const auto isTicked = entry.name == manager.getCurrentPresetName();

            if (entry.isFactory)
                factoryMenu.addItem (itemId, entry.name, true, isTicked);
            else
                userMenu.addItem (itemId, entry.name, true, isTicked);
        }

        juce::PopupMenu menu;
        menu.addSubMenu (TRANS ("Factory"), factoryMenu, factoryMenu.containsAnyActiveItems());
        menu.addSubMenu (TRANS ("User"), userMenu, userMenu.containsAnyActiveItems());
        menu.addSeparator();

        const auto setDefaultId = static_cast<int> (presetNames.size()) + 1;
        menu.addItem (setDefaultId, TRANS ("Set current as default"));

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nameButton),
                             [this, presetNames, setDefaultId] (int result)
        {
            if (result == 0)
                return;

            if (result == setDefaultId)
            {
                manager.setCurrentAsDefault();
                return;
            }

            const auto index = static_cast<size_t> (result - 1);

            if (index < presetNames.size())
                manager.loadPreset (presetNames[index]);

            refreshFromManager();
        });
    }

    void PresetBar::promptAndSaveAs()
    {
        // A bespoke rename/save dialog is M3 GUI-polish scope - this uses a
        // stock, fully-functional juce::AlertWindow text prompt instead.
        //
        // deleteWhenDismissed=false is deliberate: JUCE deletes the
        // component *before* invoking the modal callback when that flag is
        // true (see Component::enterModalState's docs), so reading
        // getTextEditorContents() from inside the callback would be a
        // use-after-free. activeAlertWindow keeps the window alive as a
        // member (the same pattern JUCE's own Projucer uses for this exact
        // scenario - jucer_NewFileWizard.cpp); the callback reads its
        // contents while it's still alive, then resets the member itself.
        activeAlertWindow = std::make_unique<juce::AlertWindow> (
            TRANS ("Save As..."), TRANS ("Enter a name for the new preset:"), juce::MessageBoxIconType::NoIcon);

        activeAlertWindow->addTextEditor ("name", manager.getCurrentPresetName(), TRANS ("Preset name"));
        activeAlertWindow->addButton (TRANS ("Save"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        activeAlertWindow->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

        activeAlertWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
        {
            if (result == 1 && activeAlertWindow != nullptr)
            {
                const auto name = activeAlertWindow->getTextEditorContents ("name");

                if (name.isNotEmpty())
                    manager.saveUserPreset (name, "Init");
            }

            activeAlertWindow.reset();
            refreshFromManager();
        }),
                                            false);
    }

    void PresetBar::promptAndImport()
    {
        activeFileChooser = std::make_unique<juce::FileChooser> (
            TRANS ("Import a preset or preset bank..."), juce::File(), "*.basilicapreset;*.zip");

        constexpr auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        activeFileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            if (file.hasFileExtension ("zip"))
            {
                manager.importBank (file);
            }
            else
            {
                juce::String errorMessage;

                if (! manager.importPresetFile (file, errorMessage))
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, TRANS ("Import failed"), errorMessage);
            }

            refreshFromManager();
        });
    }

    void PresetBar::promptAndExport()
    {
        const auto name = manager.getCurrentPresetName();

        if (name.isEmpty())
            return;

        activeFileChooser = std::make_unique<juce::FileChooser> (
            TRANS ("Export preset..."),
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name + ".basilicapreset"),
            "*.basilicapreset");

        constexpr auto flags = juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting;

        activeFileChooser->launchAsync (flags, [this, name] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file != juce::File())
                manager.exportPreset (name, file);
        });
    }

    void PresetBar::resized()
    {
        auto bounds = getLocalBounds();

        previousButton.setBounds (bounds.removeFromLeft (arrowButtonWidth));
        bounds.removeFromLeft (margin);

        exportButton.setBounds (bounds.removeFromRight (actionButtonWidth));
        bounds.removeFromRight (margin);
        importButton.setBounds (bounds.removeFromRight (actionButtonWidth));
        bounds.removeFromRight (margin);
        deleteButton.setBounds (bounds.removeFromRight (actionButtonWidth));
        bounds.removeFromRight (margin);
        saveAsButton.setBounds (bounds.removeFromRight (actionButtonWidth));
        bounds.removeFromRight (margin);
        saveButton.setBounds (bounds.removeFromRight (actionButtonWidth));
        bounds.removeFromRight (margin);

        nextButton.setBounds (bounds.removeFromRight (arrowButtonWidth));
        bounds.removeFromRight (margin);

        nameButton.setBounds (bounds);
    }
}

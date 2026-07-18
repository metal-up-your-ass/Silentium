#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PresetManager.h"

// Basilica Audio suite-wide M2 preset system: the editor half of
// PresetManager.h (.scaffold/specs/preset-system-m2.md). Copy-paste-portable
// to sibling plugins - see docs/preset-system-notes.md for the exact
// replication recipe.
//
// A horizontal strip "[<] [PresetName*] [>] [Save] [Save As...] [Delete]
// [Import...] [Export...]" built from stock juce::TextButton/PopupMenu/
// AlertWindow/FileChooser. Since the v0.3.1 visual overhaul the styling
// comes entirely from BasilicaLookAndFeel (brass 3-slice button caps, the
// preset name in a recessed dark display window selected via this
// component's "presetNameDisplay" componentID, EB Garamond type) - this
// class deliberately contains NO drawing code of its own, so the reskin
// propagates to every sibling plugin that copies the suite-shared
// src/presets/ + src/gui/ family verbatim.
namespace basilica::presets
{
    class PresetBar : public juce::Component, private juce::Timer
    {
    public:
        explicit PresetBar (PresetManager& managerToControl);
        ~PresetBar() override;

        void resized() override;

    private:
        void refreshFromManager();
        void showPresetMenu();
        void promptAndSaveAs();
        void promptAndImport();
        void promptAndExport();
        void timerCallback() override;

        PresetManager& manager;

        juce::TextButton previousButton { "<" };
        juce::TextButton nameButton; // click opens the preset menu; text shows "PresetName[*]"
        juce::TextButton nextButton { ">" };
        juce::TextButton saveButton;
        juce::TextButton saveAsButton;
        juce::TextButton deleteButton;
        juce::TextButton importButton;
        juce::TextButton exportButton;

        std::unique_ptr<juce::FileChooser> activeFileChooser;
        std::unique_ptr<juce::AlertWindow> activeAlertWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
    };
}

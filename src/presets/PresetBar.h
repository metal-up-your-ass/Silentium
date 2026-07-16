#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PresetManager.h"

// Basilica Audio suite-wide M2 preset system: the editor half of
// PresetManager.h (.scaffold/specs/preset-system-m2.md). Copy-paste-portable
// to sibling plugins - see docs/preset-system-notes.md for the exact
// replication recipe.
//
// Deliberately plain: a horizontal strip
// "[<] [PresetName*] [>] [Save] [Save As...] [Delete] [Import...]
// [Export...]" using stock juce::TextButton/PopupMenu/AlertWindow/
// FileChooser, matching the rest of this plugin's v0.1/v0.2 functional-but-
// unstyled editor. M3 (GUI & a11y milestone) restyles this; M2's job is
// correct, fully-wired behaviour, not visual polish - per the spec's own
// "do not gold-plate" note.
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

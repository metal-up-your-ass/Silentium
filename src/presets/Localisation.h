#pragma once

// Basilica Audio suite-wide M2 i18n frame (.scaffold/specs/preset-system-m2.md,
// "I18N" section). Copy-paste-portable to sibling plugins: the only
// per-plugin piece is the BinaryData symbol name passed to
// installLocalisation() from PluginEditor.cpp (see
// docs/preset-system-notes.md).
//
// Scope, per the binding spec: ALL user-facing FRAME strings (PresetBar
// labels, menu items, dialogs, error messages) are wrapped in JUCE's
// TRANS()/juce::translate(). Core/DSP terminology - parameter names, units,
// technical terms (LoCut, HiCut, Distance, IR Blend, Mix, Level, Hz, dB, %)
// - is NEVER translated anywhere in this plugin; PluginEditor.cpp's knob
// labels intentionally do not call TRANS() on parameter names.
namespace basilica::presets
{
    // Installs the correct juce::LocalisedStrings mappings for the current
    // system language: German (resources/i18n/de.txt, embedded via
    // BinaryData) if juce::SystemStats::getUserLanguage() starts with "de",
    // otherwise no mappings are installed and TRANS() falls through to its
    // built-in behaviour (return the original English string unchanged).
    // No user-facing language override yet - selection is automatic, once,
    // per the spec.
    //
    // Message-thread-only (juce::LocalisedStrings::setCurrentMappings() is
    // not documented as audio-thread-safe, and there is no reason to ever
    // call this from processBlock). Idempotent - safe to call more than
    // once (e.g. if more than one editor is opened over the plugin's
    // lifetime); each call simply reinstalls the same mappings.
    //
    // `deTranslationData`/`deTranslationDataSize` are the calling plugin's
    // own BinaryData:: symbols for resources/i18n/de.txt (e.g.
    // BinaryData::de_txt/BinaryData::de_txtSize) - passed in rather than
    // included here, so this header stays free of any BinaryData.h
    // dependency (see PresetManager.h's FactoryPresetAsset for the same
    // pattern).
    void installLocalisation (const char* deTranslationData, int deTranslationDataSize);
}

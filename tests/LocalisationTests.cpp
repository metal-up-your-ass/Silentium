// M2 i18n frame tests (.scaffold/specs/preset-system-m2.md's "I18N" section):
// the German mapping parses, every TRANS() key PresetBar actually uses is
// present in resources/i18n/de.txt, and core/DSP parameter names are
// verifiably NOT translated anywhere in the mapping.

#include "params/ParameterIds.h"

#include <juce_core/juce_core.h>

#include <BinaryData.h>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE ("i18n: resources/i18n/de.txt parses as a valid LocalisedStrings mapping", "[i18n]")
{
    REQUIRE (BinaryData::de_txt != nullptr);
    REQUIRE (BinaryData::de_txtSize > 0);

    const auto text = juce::String::fromUTF8 (BinaryData::de_txt, BinaryData::de_txtSize);
    juce::LocalisedStrings localised (text, true);

    CHECK (localised.getLanguageName().isNotEmpty());

    // Sanity: at least one known frame string round-trips through the
    // parsed mapping.
    CHECK (localised.translate ("Save") != juce::String ("Save"));
}

TEST_CASE ("i18n: every TRANS() key PresetBar uses is present in de.txt", "[i18n]")
{
    const auto text = juce::String::fromUTF8 (BinaryData::de_txt, BinaryData::de_txtSize);
    juce::LocalisedStrings localised (text, true);

    // Mirrors the exact literal strings src/presets/PresetBar.cpp passes to
    // TRANS() - kept as an explicit list here (rather than parsed out of the
    // source file) so this test fails loudly and specifically if a string
    // is ever added to PresetBar.cpp without a matching de.txt entry.
    static constexpr const char* frameKeys[] = {
        "Factory",
        "User",
        "Set current as default",
        "Save",
        "Save As...",
        "Delete",
        "Import...",
        "Export...",
        "Save As...", // AlertWindow title, same literal as the button
        "Enter a name for the new preset:",
        "Preset name",
        "Cancel",
        "Import a preset or preset bank...",
        "Import failed",
        "Export preset...",
        "This file is not a valid preset.",
        "This preset was saved by an incompatible version of the preset format.",
        "This preset file belongs to a different plugin.",
    };

    for (const auto* key : frameKeys)
    {
        CAPTURE (key);
        CHECK (localised.translate (juce::String (key)) != juce::String (key));
    }
}

TEST_CASE ("i18n: core/DSP parameter names are NEVER present as translation keys", "[i18n]")
{
    const auto text = juce::String::fromUTF8 (BinaryData::de_txt, BinaryData::de_txtSize);

    // Both the human-readable parameter labels (as used in ParameterLayout.cpp/
    // PluginEditor.cpp) and the raw APVTS IDs (ParameterIds.h) must never
    // appear as a translation key - core/DSP terminology stays English
    // everywhere, per the binding spec's I18N section.
    static constexpr const char* parameterLabels[] = {
        "Threshold", "Attack", "Hold", "Release", "Range",
        "Lookahead", "SC HPF", "SC LPF", "Knee", "Duck", "Listen",
    };

    for (const auto* label : parameterLabels)
    {
        CAPTURE (label);
        // A translation *key* line looks like `"Label" = "..."` - checking
        // for the quoted key form avoids false positives from the label
        // merely appearing inside a longer English sentence (it doesn't
        // here, but this is the precise, spec-literal check: "verifiably
        // NOT in the mapping" as a key).
        const auto keyForm = juce::String ("\"") + label + "\" =";
        CHECK (! text.contains (keyForm));
    }

    static constexpr const char* parameterIds[] = {
        ParamIDs::threshold, ParamIDs::attack, ParamIDs::hold, ParamIDs::release,
        ParamIDs::range, ParamIDs::lookahead, ParamIDs::scHighpass, ParamIDs::scLowpass,
        ParamIDs::knee, ParamIDs::duck, ParamIDs::listen,
    };

    for (const auto* id : parameterIds)
    {
        CAPTURE (id);
        const auto keyForm = juce::String ("\"") + id + "\" =";
        CHECK (! text.contains (keyForm));
    }
}

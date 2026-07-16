#include "Localisation.h"

#include <juce_core/juce_core.h>

namespace basilica::presets
{
    void installLocalisation (const char* deTranslationData, int deTranslationDataSize)
    {
        const auto userLanguage = juce::SystemStats::getUserLanguage();

        if (userLanguage.startsWithIgnoreCase ("de") && deTranslationData != nullptr && deTranslationDataSize > 0)
        {
            const auto text = juce::String::fromUTF8 (deTranslationData, deTranslationDataSize);

            // setCurrentMappings() takes ownership of the pointer passed to
            // it (see juce_LocalisedStrings.h) - this is the documented
            // JUCE idiom, not a leak.
            juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (text, true));
        }
        else
        {
            juce::LocalisedStrings::setCurrentMappings (nullptr);
        }
    }
}

// All other test files are picked up via the glob in CMakeLists.txt.
// We link only Catch2::Catch2 (not Catch2::Catch2WithMain) so that we can
// initialise JUCE's GUI/MessageManager singleton before any test runs -
// AudioProcessorValueTreeState and other JUCE utilities rely on it being
// present even in a headless console test executable.

#include <juce_gui_basics/juce_gui_basics.h>

#include <catch2/catch_session.hpp>

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceGuiInitialiser;

    return Catch::Session().run (argc, argv);
}

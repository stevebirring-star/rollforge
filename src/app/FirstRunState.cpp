#include "app/FirstRunState.h"

namespace rollforge
{
namespace FirstRunState
{

juce::File markerFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("RollForge")
               .getChildFile ("welcome.done");
}

bool shouldShow (const juce::File& marker)
{
    return ! marker.existsAsFile();
}

void markShown (const juce::File& marker)
{
    marker.getParentDirectory().createDirectory();
    marker.replaceWithText ("1");   // content is irrelevant; the file's presence is the signal
}

bool shouldShow() { return shouldShow (markerFile()); }
void markShown()  { markShown (markerFile()); }

} // namespace FirstRunState
} // namespace rollforge

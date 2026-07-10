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

juce::File tourMarkerFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("RollForge")
               .getChildFile ("tour.done");
}

bool shouldShow() { return shouldShow (markerFile()); }
void markShown()  { markShown (markerFile()); }

bool shouldShowTour()  { return shouldShow (tourMarkerFile()); }
void markTourShown()   { markShown (tourMarkerFile()); }
void clearTourShown()  { tourMarkerFile().deleteFile(); }

} // namespace FirstRunState
} // namespace rollforge

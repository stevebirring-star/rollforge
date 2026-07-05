#include "app/Autosave.h"

#include "model/ProjectIO.h"

namespace rollforge
{
namespace Autosave
{

juce::File recoveryFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("RollForge")
               .getChildFile ("recovery.rollforge");
}

bool save (const Project& project, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    return ProjectIO::save (project, file);
}

bool hasRecovery (const juce::File& file)
{
    return file.existsAsFile() && file.getSize() > 2;
}

bool load (const juce::File& file, Project& out)
{
    return ProjectIO::load (file, out);
}

void clear (const juce::File& file)
{
    file.deleteFile();
}

bool save (const Project& project) { return save (project, recoveryFile()); }
bool hasRecovery()                 { return hasRecovery (recoveryFile()); }
bool load (Project& out)           { return load (recoveryFile(), out); }
void clear()                       { clear (recoveryFile()); }

} // namespace Autosave
} // namespace rollforge

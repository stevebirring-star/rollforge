#include "app/AppSettings.h"

namespace rollforge
{

juce::String AppSettings::toJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("uiScale", uiScale);

    juce::Array<juce::var> folders;
    for (const auto& f : sampleFolders)
        folders.add (f);
    root->setProperty ("sampleFolders", folders);

    return juce::JSON::toString (juce::var (root));
}

AppSettings AppSettings::fromJson (const juce::String& json)
{
    AppSettings s;
    const juce::var root = juce::JSON::parse (json);
    if (! root.isObject())
        return s;

    s.uiScale = (float) (double) root.getProperty ("uiScale", 1.0);
    if (s.uiScale < 0.5f) s.uiScale = 0.5f;
    if (s.uiScale > 3.0f) s.uiScale = 3.0f;

    if (auto* folders = root.getProperty ("sampleFolders", juce::var()).getArray())
        for (const auto& f : *folders)
            s.sampleFolders.add (f.toString());

    return s;
}

juce::File AppSettings::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("RollForge")
               .getChildFile ("settings.json");
}

bool AppSettings::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (toJson());
}

AppSettings AppSettings::loadFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};
    return fromJson (file.loadFileAsString());
}

bool AppSettings::save() const   { return saveTo (settingsFile()); }
AppSettings AppSettings::load()  { return loadFrom (settingsFile()); }

} // namespace rollforge

// RollForge — AppSettings tests (Phase 7, commit 2).
//
// Headless: settings JSON round-trips (UI scale + sample folders); file save/load
// works and a missing file yields defaults.

#include "app/AppSettings.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class AppSettingsTest final : public juce::UnitTest
{
public:
    AppSettingsTest() : juce::UnitTest ("RollForge AppSettings", testCategory) {}

    void runTest() override
    {
        beginTest ("settings JSON round-trips");
        {
            AppSettings s;
            s.uiScale = 1.25f;
            s.sampleFolders.add ("/a/kicks");
            s.sampleFolders.add ("/b/snares");

            const auto t = AppSettings::fromJson (s.toJson());
            expectWithinAbsoluteError (t.uiScale, 1.25f, 1.0e-4f);
            expectEquals (t.sampleFolders.size(), 2);
            expect (t.sampleFolders[0] == juce::String ("/a/kicks"));
            expect (t.sampleFolders[1] == juce::String ("/b/snares"));
        }

        beginTest ("file round-trip + defaults for a missing file");
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_settings_test.json");
            f.deleteFile();

            const auto def = AppSettings::loadFrom (f);
            expectWithinAbsoluteError (def.uiScale, 1.0f, 1.0e-4f);
            expectEquals (def.sampleFolders.size(), 0);

            AppSettings s;
            s.uiScale = 1.5f;
            s.sampleFolders.add ("/x");
            expect (s.saveTo (f));

            const auto r = AppSettings::loadFrom (f);
            expectWithinAbsoluteError (r.uiScale, 1.5f, 1.0e-4f);
            expectEquals (r.sampleFolders.size(), 1);
            expect (r.sampleFolders[0] == juce::String ("/x"));

            f.deleteFile();
        }
    }
};

static AppSettingsTest appSettingsTest;

} // namespace rollforge::tests

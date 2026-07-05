// RollForge — FirstRunState tests (Phase 7, commit 3).
//
// Headless: the welcome overlay is offered until the marker is written, then never
// again; writing the marker is idempotent; clearing the marker re-arms it.

#include "app/FirstRunState.h"

#include <juce_core/juce_core.h>

namespace rollforge::tests
{

static constexpr const char* testCategory = "rollforge";

class FirstRunStateTest final : public juce::UnitTest
{
public:
    FirstRunStateTest() : juce::UnitTest ("RollForge FirstRunState", testCategory) {}

    void runTest() override
    {
        beginTest ("welcome shows until the marker is written, then never again");
        {
            auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("rollforge_welcome_test.done");
            f.deleteFile();
            expect (FirstRunState::shouldShow (f));      // absent -> show

            FirstRunState::markShown (f);
            expect (! FirstRunState::shouldShow (f));     // present -> don't show

            FirstRunState::markShown (f);                 // idempotent: still marked
            expect (! FirstRunState::shouldShow (f));

            f.deleteFile();
            expect (FirstRunState::shouldShow (f));      // cleared -> re-armed

            f.deleteFile();
        }
    }
};

static FirstRunStateTest firstRunStateTest;

} // namespace rollforge::tests

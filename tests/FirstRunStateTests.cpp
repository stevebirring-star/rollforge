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

        beginTest ("the tour has its own marker, so the welcome does not silence it");
        {
            // Someone who opened the app before the tour existed has welcome.done already.
            // Sharing that marker would deny the tour to exactly the people who need it.
            expect (FirstRunState::markerFile() != FirstRunState::tourMarkerFile(),
                    "the welcome and the tour share a marker file");
            expectEquals (FirstRunState::tourMarkerFile().getFileName(), juce::String ("tour.done"));
            expectEquals (FirstRunState::markerFile().getFileName(), juce::String ("welcome.done"));

            const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("RollForgeTourGate")
                                 .getChildFile (juce::Uuid().toString());
            dir.createDirectory();
            const auto welcome = dir.getChildFile ("welcome.done");
            const auto tour    = dir.getChildFile ("tour.done");

            FirstRunState::markShown (welcome);
            expect (! FirstRunState::shouldShow (welcome), "the welcome is done");
            expect (FirstRunState::shouldShow (tour), "the tour must still be offered");

            FirstRunState::markShown (tour);
            expect (! FirstRunState::shouldShow (tour));

            // Help re-arms it: clearing the marker offers it again, and only it.
            tour.deleteFile();
            expect (FirstRunState::shouldShow (tour), "the tour cannot be re-run from Help");
            expect (! FirstRunState::shouldShow (welcome), "re-arming the tour reopened the welcome");

            dir.deleteRecursively();
        }
    }
};

static FirstRunStateTest firstRunStateTest;

} // namespace rollforge::tests

#pragma once

// RollForge — FirstRunState: the "has the welcome been shown yet?" gate. A tiny
// marker file under app-data records that the one-time welcome overlay has been
// dismissed, so it never reappears. Pure file logic (juce_core) so it is
// headless-testable; the FirstRun overlay component + its wiring live in the UI.

#include <juce_core/juce_core.h>

namespace rollforge
{
namespace FirstRunState
{
    /** <user-app-data>/RollForge/welcome.done */
    juce::File markerFile();

    // Explicit-file forms (used by tests).
    bool shouldShow (const juce::File& marker);   // true while the marker is absent
    void markShown (const juce::File& marker);    // create the marker (idempotent)

    // Convenience forms using markerFile().
    bool shouldShow();
    void markShown();
}
} // namespace rollforge

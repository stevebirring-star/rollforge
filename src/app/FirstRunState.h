#pragma once

// RollForge — FirstRunState: the "has this been shown yet?" gate. A tiny marker file under
// app-data records that a one-time overlay has been dismissed, so it never reappears. Pure
// file logic (juce_core) so it is headless-testable; the overlays live in the UI.
//
// The welcome panel and the guided tour have SEPARATE markers. Reusing welcome.done would
// mean nobody who had already opened the app once would ever be offered the tour that shipped
// after they did — which is exactly the set of people who have questions it answers.

#include <juce_core/juce_core.h>

namespace rollforge
{
namespace FirstRunState
{
    /** <user-app-data>/RollForge/welcome.done */
    juce::File markerFile();

    /** <user-app-data>/RollForge/tour.done — the guided coach-mark tour. */
    juce::File tourMarkerFile();

    // Explicit-file forms (used by tests).
    bool shouldShow (const juce::File& marker);   // true while the marker is absent
    void markShown (const juce::File& marker);    // create the marker (idempotent)

    // Convenience forms using markerFile().
    bool shouldShow();
    void markShown();

    // ...and using tourMarkerFile().
    bool shouldShowTour();
    void markTourShown();

    /** Re-arms the tour so Help can offer it again. */
    void clearTourShown();
}
} // namespace rollforge

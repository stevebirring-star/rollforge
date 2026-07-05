#pragma once

// RollForge — Autosave: crash recovery. The app writes a recovery `.rollforge`
// every ~60 s and clears it on a clean exit; if a recovery file is present at
// launch (i.e. the last session did NOT exit cleanly), the app restores it.
//
// This is the pure file logic (juce_core + ProjectIO) so it is headless-testable;
// the periodic timer + restore prompt live in the app/UI (no juce_events here, so
// the headless build stays glib-free).

#include "model/Project.h"

#include <juce_core/juce_core.h>

namespace rollforge
{
namespace Autosave
{
    /** <user-app-data>/RollForge/recovery.rollforge */
    juce::File recoveryFile();

    // Explicit-file forms (used by tests).
    bool save (const Project& project, const juce::File& file);
    bool hasRecovery (const juce::File& file);          // present + non-empty
    bool load (const juce::File& file, Project& out);
    void clear (const juce::File& file);

    // Convenience forms using recoveryFile().
    bool save (const Project& project);
    bool hasRecovery();
    bool load (Project& out);
    void clear();
}
} // namespace rollforge

#pragma once

// RollForge — ProjectIO: `.rollforge` JSON save/load of a Project. Pure model
// (juce_core JSON), fully headless-testable via a save->load round-trip.

#include "model/Project.h"

#include <juce_core/juce_core.h>

namespace rollforge
{
namespace ProjectIO
{
    /** Serialises a project to a JSON string. */
    juce::String toJson (const Project& project);

    /** Parses a project from a JSON string. Returns false on malformed input. */
    bool fromJson (const juce::String& json, Project& out);

    /** Writes / reads the JSON to / from a `.rollforge` file. */
    bool save (const Project& project, const juce::File& file);
    bool load (const juce::File& file, Project& out);
}
} // namespace rollforge

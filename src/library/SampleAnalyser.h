#pragma once

// RollForge — SampleAnalyser: decode one audio file into a LibraryEntry.
//
// Split out of Scanner because it is the only part of scanning that has nothing to do with a
// database. The folder watcher runs it on a background thread, the "Similar" button runs it on
// a file the library has never seen, and Scanner runs it in a loop — none of which should have
// to hold a LibraryDb reference to do it.
//
// It owns a SampleLoader (an AudioFormatManager is expensive to build), so give each thread
// its own. Everything it touches is thread-local; nothing here is shared.
//
// LIBRARY LAYER: juce_core + juce_audio_formats, no GUI, never the audio thread.

#include "library/LibraryDb.h"
#include "library/SampleLoader.h"

namespace rollforge
{

class SampleAnalyser
{
public:
    // Declaring the copy constructor (below) suppresses the implicit default one.
    SampleAnalyser() = default;

    /** Decode `file`, measure it, categorise it, and fill `out` (path, name, features,
        category). No DB write. False when the file cannot be decoded. */
    bool analyse (const juce::File& file, LibraryEntry& out);

    /** The file extensions this analyser can read, as a wildcard list. */
    juce::String supportedWildcards() const { return loader.getSupportedWildcards(); }

private:
    SampleLoader loader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleAnalyser)
};

} // namespace rollforge

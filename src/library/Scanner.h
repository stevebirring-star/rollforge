#pragma once

// RollForge — Scanner: walks a folder for audio files, decodes + analyses +
// categorises each, and upserts it into a LibraryDb. Runs OFF the audio thread
// (message / background); the browser wraps scanBlocking() in a thread pool with
// progress. juce_core + juce_audio_formats (no GUI) so it is headless-testable.

#include "library/LibraryDb.h"
#include "library/SampleLoader.h"

#include <juce_core/juce_core.h>

#include <atomic>

namespace rollforge
{

class Scanner
{
public:
    explicit Scanner (LibraryDb& db);

    /** Recursively scan `folder` synchronously; decode/analyse/categorise/upsert
        every supported audio file. Returns the number of files stored. */
    int scanBlocking (const juce::File& folder);

    /** Decode one file and fill `out` (path, name, features, category). No DB
        write. Returns false if the file can't be decoded. */
    bool analyseFile (const juce::File& file, LibraryEntry& out);

    /** Files visited so far in the current scan (any thread). */
    int getScannedCount() const noexcept { return scanned.load (std::memory_order_acquire); }

private:
    LibraryDb&       db;
    SampleLoader     loader;
    std::atomic<int> scanned { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Scanner)
};

} // namespace rollforge

#pragma once

// RollForge — LibraryDb: a thin C++ wrapper over the vendored SQLite amalgamation
// (src/library/sqlite/). Stores one row per analysed sample (path + name +
// features + category + favourite) and answers the browser's queries. Message-
// thread / scanner-thread use only (NOT the audio thread). juce_core only (no GUI)
// so it is headless-testable.

#include "library/Categoriser.h"

#include <juce_core/juce_core.h>

#include <vector>

struct sqlite3;   // fwd-declared to keep the 9 MB sqlite3.h out of this header

namespace rollforge
{

struct LibraryEntry
{
    juce::String  path;
    juce::String  name;
    float         durationSeconds = 0.0f;
    float         rms             = 0.0f;
    float         zcr             = 0.0f;
    float         decay           = 0.0f;
    int           onsetCount      = 0;
    SoundCategory category        = SoundCategory::Unknown;
    float         confidence      = 1.0f;
    bool          favourite       = false;
};

class LibraryDb
{
public:
    LibraryDb() = default;
    ~LibraryDb();

    bool open (const juce::File& dbFile);   // opens/creates the DB file + schema
    bool openInMemory();                     // ":memory:" — for tests
    void close();
    bool isOpen() const noexcept { return db != nullptr; }

    bool upsert (const LibraryEntry& entry);                     // insert or replace by path
    bool setFavourite (const juce::String& path, bool favourite);

    // Manual re-tag: a persistent per-path category override, kept in its own table
    // so it SURVIVES a re-scan (which replaces the samples row) and is consulted by
    // every query. Correcting a misjudged sample sticks.
    bool setCategoryOverride (const juce::String& path, SoundCategory category);
    bool clearCategoryOverride (const juce::String& path);

    // Folders the watcher keeps an eye on. Persisted, because a library you have to re-add
    // on every launch is not a library. Adding one twice is not an error.
    bool addWatchedFolder (const juce::String& path);
    bool removeWatchedFolder (const juce::String& path);
    juce::StringArray watchedFolders() const;

    /** Just the paths, for the watcher's "have I seen this file?" set. */
    juce::StringArray allPaths() const;

    int  count() const;

    std::vector<LibraryEntry> all() const;
    std::vector<LibraryEntry> byCategory (SoundCategory category) const;
    std::vector<LibraryEntry> favourites() const;

private:
    bool openAt (const char* filename);
    bool createSchema();
    std::vector<LibraryEntry> queryWhere (const juce::String& whereClause) const;

    sqlite3* db = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryDb)
};

} // namespace rollforge

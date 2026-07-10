#pragma once

// RollForge — FolderWatcher: the library keeps itself up to date.
//
// Point it at your samples folder once. Everything under it is analysed in the background,
// and anything you drop in later is picked up on its own — no "rescan" button to remember,
// no frozen window while a thousand kicks are decoded. The folders are persisted, so it is
// still watching them tomorrow.
//
// THE THREADING RULE, AND THE WHOLE REASON THIS CLASS EXISTS:
//
//     The worker thread never touches the database.
//
// It enumerates files, decodes them and measures them — all of which is slow, and none of
// which needs a LibraryDb. Finished rows go into a queue; the message thread drains that
// queue and does every write. So there is no second connection, no writer blocking a reader,
// and no question about what SQLITE_THREADSAFE buys us. The worker holds a set of paths it
// has already dealt with, so a row waiting in the queue is not analysed twice.
//
// POLLING, NOT INOTIFY. There is no cross-platform filesystem-watch API in JUCE, and a
// hand-rolled one per platform is three times the surface area for a feature whose worst
// case is noticing a new file four seconds late. The worker re-enumerates on a timer.
//
// WHAT IT DOES NOT DO, deliberately:
//   * It ingests NEW paths only. Replacing a file in place keeps the features the library
//     already measured for it; re-tag or re-add to refresh.
//   * It never deletes rows. A folder on an unmounted drive looks exactly like a folder whose
//     files were all deleted, and one of those two is not a reason to throw a library away.
//
// It owns no timer and broadcasts nothing. The message thread already has a timer; it calls
// drainIntoDb() from there. That keeps this class in the library layer (juce_core only), and
// means the one place rows enter the database is a function the caller chose to call.
//
// LIBRARY LAYER: no GUI, no juce_events. Owned by the message thread.

#include "library/LibraryDb.h"
#include "library/SampleAnalyser.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <set>
#include <vector>

namespace rollforge
{

class FolderWatcher final : private juce::Thread
{
public:
    /** `db` must outlive this. Nothing starts until start() is called. */
    explicit FolderWatcher (LibraryDb& db);
    ~FolderWatcher() override;

    /** Loads the watched folders from the DB and starts the worker. Nothing reaches the
        database until someone calls drainIntoDb(). */
    void start();

    /** Adds a folder, persists it, and wakes the worker. Watching a folder twice is a no-op. */
    void addFolder (const juce::File& folder);

    /** Stops watching. The samples already ingested from it are kept: they are still on disk,
        and forgetting a folder is not the same as wanting your kicks deleted. */
    void removeFolder (const juce::File& folder);

    juce::StringArray folders() const;

    /** Wake the worker now instead of at the next poll. */
    void poke();

    bool isBusy()      const noexcept { return busy.load (std::memory_order_acquire); }
    int  filesFound()  const noexcept { return found.load (std::memory_order_acquire); }
    int  filesDone()   const noexcept { return done.load (std::memory_order_acquire); }

    /** Total samples written to the DB since start(), for the browser's status line. It
        changes only inside drainIntoDb(), so a UI can poll it to know when to refresh. */
    int  totalAdded()  const noexcept { return added.load (std::memory_order_acquire); }

    /** Completed enumerate-and-analyse passes since start(). */
    int  scanPasses() const noexcept { return passes.load (std::memory_order_acquire); }

    /** Rows analysed and waiting for the message thread to write them. */
    int  pendingRows() const;

    /** MESSAGE THREAD. Writes up to `maxRows` finished rows into the DB and returns how many
        it wrote. Call it from whatever timer you already have; the batch cap is what keeps a
        fifty-thousand-file import from stalling the UI for a second. */
    int drainIntoDb (int maxRows = 200);

    //==============================================================================
    // For tests.

    /** Wakes the worker and blocks until it has completed a whole pass that began AFTER this
        call — two passes, because the poke may land while one is already running. False on
        timeout. */
    bool waitForScanPass (int timeoutMs);

    /** Milliseconds between polls. Exposed so a test does not have to wait four seconds. */
    void setPollIntervalMs (int ms) noexcept { pollMs = juce::jmax (10, ms); }

private:
    void run() override;   // worker
    void scanOnce();       // worker: enumerate + analyse everything new

    LibraryDb& db;

    mutable juce::CriticalSection lock;   // guards folderList, seen and results
    juce::StringArray             folderList;
    std::set<juce::String>        seen;    // paths in the DB, or already in `results`
    std::vector<LibraryEntry>     results;

    SampleAnalyser analyser;              // worker-thread only

    std::atomic<bool> busy   { false };
    std::atomic<int>  found  { 0 };
    std::atomic<int>  done   { 0 };
    std::atomic<int>  added  { 0 };
    std::atomic<int>  passes { 0 };
    std::atomic<int>  pollMs { 4000 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderWatcher)
};

} // namespace rollforge

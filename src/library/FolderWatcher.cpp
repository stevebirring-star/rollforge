#include "library/FolderWatcher.h"

namespace rollforge
{

FolderWatcher::FolderWatcher (LibraryDb& dbToUse)
    : juce::Thread ("RollForge library watcher"), db (dbToUse)
{
}

FolderWatcher::~FolderWatcher()
{
    // notify() as well as the exit flag: the worker spends nearly all its life inside wait().
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

void FolderWatcher::start()
{
    {
        const juce::ScopedLock sl (lock);
        folderList = db.watchedFolders();

        seen.clear();
        for (const auto& p : db.allPaths())
            seen.insert (p);
    }

    startThread (juce::Thread::Priority::background);
    poke();
}

void FolderWatcher::addFolder (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;

    const auto path = folder.getFullPathName();
    db.addWatchedFolder (path);

    {
        const juce::ScopedLock sl (lock);
        folderList.addIfNotAlreadyThere (path);
    }
    poke();
}

void FolderWatcher::removeFolder (const juce::File& folder)
{
    const auto path = folder.getFullPathName();
    db.removeWatchedFolder (path);

    const juce::ScopedLock sl (lock);
    folderList.removeString (path);
}

juce::StringArray FolderWatcher::folders() const
{
    const juce::ScopedLock sl (lock);
    return folderList;
}

void FolderWatcher::poke()
{
    notify();
}

//==============================================================================
void FolderWatcher::run()
{
    while (! threadShouldExit())
    {
        scanOnce();
        passes.fetch_add (1, std::memory_order_acq_rel);

        if (threadShouldExit())
            break;

        wait (pollMs.load (std::memory_order_acquire));
    }
}

void FolderWatcher::scanOnce()
{
    juce::StringArray toScan;
    {
        const juce::ScopedLock sl (lock);
        toScan = folderList;
    }

    if (toScan.isEmpty())
        return;

    // Busy from here, not from the first decode: enumerating a deep folder is itself slow, and
    // a progress line that only appears once decoding starts leaves the window looking hung.
    busy.store (true, std::memory_order_release);
    const juce::ScopeGuard clearBusy { [this] { busy.store (false, std::memory_order_release); } };

    const auto wildcards = analyser.supportedWildcards();

    // Enumerate first, so `found` is a real total and the progress bar is not a lie. This is
    // the cheap half; decoding is the expensive one.
    juce::Array<juce::File> candidates;
    for (const auto& folderPath : toScan)
    {
        if (threadShouldExit())
            return;

        const juce::File folder (folderPath);
        if (! folder.isDirectory())
            continue;   // an unmounted drive: skip it, and keep every row we have from it

        juce::Array<juce::File> files;
        folder.findChildFiles (files, juce::File::findFiles, true, wildcards);

        for (const auto& f : files)
        {
            const juce::ScopedLock sl (lock);
            if (seen.find (f.getFullPathName()) == seen.end())
                candidates.add (f);
        }
    }

    if (candidates.isEmpty())
    {
        found.store (0, std::memory_order_release);
        done.store (0, std::memory_order_release);
        return;
    }

    found.store (candidates.size(), std::memory_order_release);
    done.store (0, std::memory_order_release);

    for (const auto& file : candidates)
    {
        if (threadShouldExit())
            break;

        LibraryEntry entry;
        if (analyser.analyse (file, entry))
        {
            const juce::ScopedLock sl (lock);

            // Claim the path here, not when the row reaches the DB. Otherwise the next poll
            // would find it again while it is still sitting in the queue, and analyse it twice.
            seen.insert (entry.path);
            results.push_back (std::move (entry));
        }

        done.fetch_add (1, std::memory_order_acq_rel);
    }
}

//==============================================================================
int FolderWatcher::drainIntoDb (int maxRows)
{
    std::vector<LibraryEntry> batch;
    {
        const juce::ScopedLock sl (lock);
        if (results.empty())
            return 0;

        const auto n = (std::size_t) juce::jlimit (1, (int) results.size(), maxRows);
        batch.assign (std::make_move_iterator (results.begin()),
                      std::make_move_iterator (results.begin() + (long) n));
        results.erase (results.begin(), results.begin() + (long) n);
    }

    int written = 0;
    for (const auto& e : batch)
        if (db.upsert (e))
            ++written;

    added.fetch_add (written, std::memory_order_acq_rel);
    return written;
}

int FolderWatcher::pendingRows() const
{
    const juce::ScopedLock sl (lock);
    return (int) results.size();
}

bool FolderWatcher::waitForScanPass (int timeoutMs)
{
    // Two passes, not one. The poke may arrive while a pass is already halfway through the
    // folder, and that pass never looked at the file the caller just wrote.
    const int target = passes.load (std::memory_order_acquire) + 2;
    const auto deadline = juce::Time::getMillisecondCounter()
                            + (juce::uint32) juce::jmax (0, timeoutMs);

    while (passes.load (std::memory_order_acquire) < target)
    {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;

        poke();
        juce::Thread::sleep (5);
    }
    return true;
}

} // namespace rollforge

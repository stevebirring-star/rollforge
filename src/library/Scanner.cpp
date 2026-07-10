#include "library/Scanner.h"

namespace rollforge
{

Scanner::Scanner (LibraryDb& dbToUse) : db (dbToUse) {}

int Scanner::scanBlocking (const juce::File& folder)
{
    scanned.store (0, std::memory_order_release);
    if (! folder.isDirectory())
        return 0;

    // noCycles: a symlink loop under the chosen folder would otherwise never terminate.
    juce::Array<juce::File> files;
    folder.findChildFiles (files, juce::File::findFiles, true, analyser.supportedWildcards(),
                           juce::File::FollowSymlinks::noCycles);

    int stored = 0;
    for (const auto& f : files)
    {
        LibraryEntry e;
        if (analyseFile (f, e) && db.upsert (e))
            ++stored;
        scanned.fetch_add (1, std::memory_order_acq_rel);
    }
    return stored;
}

} // namespace rollforge

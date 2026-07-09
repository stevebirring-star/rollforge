#pragma once

// RollForge — BrowserPanel: the sample-library browser. Watch a folder (the FolderWatcher
// analyses it in the background and keeps it up to date), filter by category, and hit NEW KIT
// to load a coherent random kit.
//
// The LibraryDb and the FolderWatcher are owned by MainComponent and passed in: the pad grid
// needs the library too (per-pad "Similar", the New Sounds reroll), and the watcher has to
// keep ingesting while this dialog is closed. A re-tag fires onLibraryChanged so the owner can
// re-normalise its similarity space; new samples arrive through the watcher instead, and this
// panel notices them by polling totalAdded().
//
// Scanning is synchronous for now (blocks briefly); a background-thread scan with
// live progress is a follow-up.
//
// Click a row to audition it. Right-click for the per-sample actions: send it to a
// pad, slice it across the pads, or re-tag its category.
//
// MAP flips the same library into a constellation (SimilarityMap): every sample a dot,
// placed by how it sounds. Both views audition on a click and share the same right-click
// menu, so they are two readings of one library rather than two browsers. (They keep
// separate selections: the list indexes the filtered rows, the map the whole corpus.)
//
// "Send to pad" rather than drag-to-pad: this panel lives in a DialogWindow, which
// launchAsync() puts into a modal state, and a JUCE internal drag cannot reach a
// component outside the modal window. A menu is deterministic and does the same job.

#include "library/FolderWatcher.h"
#include "library/KitBuilder.h"
#include "library/LibraryDb.h"
#include "ui/SimilarityMap.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace rollforge
{

class BrowserPanel final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::Timer
{
public:
    BrowserPanel (LibraryDb& db, FolderWatcher& watcher);
    ~BrowserPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Fired by NEW KIT with the chosen sample path per pad ("" = leave pad). */
    std::function<void (const std::array<juce::String, kitNumPads>&)> onNewKit;

    /** Fired on a plain click: play this sample through the engine's preview pad. */
    std::function<void (const juce::String& path)> onAudition;

    /** Fired by "Send to pad": load this sample into that pad, replacing what's there. */
    std::function<void (const juce::String& path, int padIndex)> onSendToPad;

    /** Fired by "Slice across the pads": chop this loop at its onsets. */
    std::function<void (const juce::String& path)> onSliceLoop;

    /** Fired whenever the corpus changes (a scan, a re-tag): the owner's cached
        similarity space is now stale. */
    std::function<void()> onLibraryChanged;

private:
    void showFoldersMenu();
    void chooseFolderToWatch();
    void timerCallback() override;   // polls the watcher for progress + new samples
    void refresh();
    void rebuildKit();
    void setMapView (bool showMap);

    /** The per-sample actions, shared by both views. `target` anchors the popup. */
    void showMenuFor (const LibraryEntry& entry, juce::Component* target);

    /** Whichever entry the list's row `row` is showing, or nullptr. */
    const LibraryEntry* filteredEntry (int row) const;

    std::optional<SoundCategory> selectedCategory() const;

    // juce::ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;   // click = audition, right = menu

    LibraryDb&                db;
    FolderWatcher&            watcher;
    std::vector<LibraryEntry> allEntries;   // the whole library: what the map plots
    std::vector<LibraryEntry> entries;      // filtered by category: what the list shows

    juce::TextButton foldersButton { "Folders..." };
    juce::TextButton newKitButton { "NEW KIT" };
    juce::ComboBox   categoryFilter;
    juce::ListBox    list;
    SimilarityMap    map;
    juce::TextButton viewButton { "Map" };   // toggles list <-> constellation
    juce::Label      statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    std::uint64_t                      kitSeed = 1;

    int  lastAdded = 0;      // watcher.totalAdded() at the last refresh
    bool wasBusy   = false;  // to catch the moment a scan finishes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace rollforge

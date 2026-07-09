#pragma once

// RollForge — BrowserPanel: the sample-library browser. Scan a folder (populates
// the SQLite library DB), filter by category, and hit NEW KIT to load a coherent
// random kit.
//
// The LibraryDb is owned by MainComponent and passed in, because the pad grid needs it
// too (per-pad "Similar", and the New Sounds reroll) and this panel only exists while its
// dialog is open. Anything that changes the corpus fires onLibraryChanged so the owner can
// re-normalise its similarity space.
//
// Scanning is synchronous for now (blocks briefly); a background-thread scan with
// live progress is a follow-up.
//
// Click a row to audition it. Right-click for the per-sample actions: send it to a
// pad, slice it across the pads, or re-tag its category.
//
// "Send to pad" rather than drag-to-pad: this panel lives in a DialogWindow, which
// launchAsync() puts into a modal state, and a JUCE internal drag cannot reach a
// component outside the modal window. A menu is deterministic and does the same job.

#include "library/KitBuilder.h"
#include "library/LibraryDb.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace rollforge
{

class BrowserPanel final : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit BrowserPanel (LibraryDb& db);
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
    void chooseFolderAndScan();
    void refresh();
    void rebuildKit();
    void showRowMenu (int row);

    // juce::ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;   // click = audition, right = menu

    LibraryDb&                db;
    std::vector<LibraryEntry> entries;

    juce::TextButton scanButton   { "Scan Folder..." };
    juce::TextButton newKitButton { "NEW KIT" };
    juce::ComboBox   categoryFilter;
    juce::ListBox    list;
    juce::Label      statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    std::uint64_t                      kitSeed = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace rollforge

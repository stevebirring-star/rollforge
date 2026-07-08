#pragma once

// RollForge — BrowserPanel: the sample-library browser. Scan a folder (populates
// the SQLite library DB), filter by category, and hit NEW KIT to load a coherent
// random kit. Owns its own LibraryDb (persisted under the user app-data dir).
//
// Scanning is synchronous for now (blocks briefly); a background-thread scan with
// live progress is a follow-up. Audition-on-click + drag->pad are also deferred.

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
    BrowserPanel();
    ~BrowserPanel() override;

    void resized() override;

    /** Fired by NEW KIT with the chosen sample path per pad ("" = leave pad). */
    std::function<void (const std::array<juce::String, kitNumPads>&)> onNewKit;

private:
    void chooseFolderAndScan();
    void refresh();
    void rebuildKit();

    // juce::ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;   // right-click = re-tag

    LibraryDb                 db;
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

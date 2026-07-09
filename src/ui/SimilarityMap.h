#pragma once

// RollForge — SimilarityMap: the library as a constellation instead of a list.
//
// Every scanned sample is a dot, placed by PCA over the features the library already
// stores, and coloured by its drum category. Sounds that sit near each other really are
// near each other — the same normalised space that answers a pad's "Similar" button
// places these dots, so what you see and what that button does can never disagree.
//
// It earns its place next to the list by answering a question the list cannot: "what else
// is over there?" A list orders on one column at a time. A map shows the shape of a
// library — that a pack has forty near-identical closed hats and two claps, that the kicks
// split into a boomy cluster and a clicky one.
//
// THE MAP NEVER MOVES UNDER A FILTER. Filtering to Kicks would let the projection re-fit
// to just those points and scatter them across the whole view, which reads as the library
// rearranging itself. Instead every point keeps its place and the ones filtered out fade
// to context. The user's memory of where their sounds live is worth more than filling the
// canvas.
//
// UI only: it is handed a corpus and its projection, and reports clicks back out.

#include "library/LibraryDb.h"
#include "library/Similarity.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <vector>

namespace rollforge
{

class SimilarityMap final : public juce::Component
{
public:
    SimilarityMap();

    /** Index-aligned corpus + projection. Pass an empty corpus to clear. */
    void setCorpus (const std::vector<LibraryEntry>& entries, const Similarity::Projection&);

    /** Draw everything, but only these are solid. Pass nullopt for "all of them". */
    void setCategoryFilter (std::optional<SoundCategory> category);

    /** Ring this one. -1 for none. */
    void setSelected (int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int index)> onAudition;     // left click a dot
    std::function<void (int index)> onContextMenu;  // right click a dot

private:
    bool isDimmed (int index) const;
    int  indexAt (juce::Point<float> position) const;   // -1 if nothing is close enough
    juce::Point<float> positionOf (int index) const;
    juce::Rectangle<float> plotArea() const;

    std::vector<LibraryEntry> corpus;
    Similarity::Projection    projection;

    std::optional<SoundCategory> filter;
    int selected = -1;
    int hovered  = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimilarityMap)
};

} // namespace rollforge

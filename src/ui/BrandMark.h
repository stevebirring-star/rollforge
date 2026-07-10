#pragma once

// RollForge — BrandMark: the wordmark, drawn rather than bundled.
//
// "ROLL" in bone, "FORGE" in hot orange, with an anvil-and-sparks glyph struck into the
// panel beside it. Drawn as text + Path, so there is no binary asset to ship, it stays
// crisp at every UI scale, and it needs no font licence.
//
// Brand is monochrome plus exactly one accent. It is never multicolour, never gradient,
// never bevelled.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rollforge
{

class BrandMark final : public juce::Component
{
public:
    BrandMark();

    void paint (juce::Graphics&) override;

private:
    /** The anvil silhouette + two rising sparks, as a unit-square Path. */
    static juce::Path anvilGlyph();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrandMark)
};

} // namespace rollforge

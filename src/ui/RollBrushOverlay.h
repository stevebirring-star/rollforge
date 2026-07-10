#pragma once

// RollForge — RollBrushOverlay: a transparent layer sitting exactly over the
// SequencerGrid's step area. When the brush is off it passes clicks through to the
// grid (normal step editing); when on, a horizontal click-drag paints one roll
// block on a lane (drag left/right = span; drag up = denser end). It also draws
// the existing roll blocks. It owns no model state — it reports painted rolls up
// via onRollPainted and renders whatever rects the owner hands back. UI only.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace rollforge
{

class RollBrushOverlay final : public juce::Component
{
public:
    RollBrushOverlay (int numLanes, int numSteps, int labelWidth);

    void setBrushEnabled (bool shouldBeEnabled);
    bool isBrushEnabled() const noexcept { return brushEnabled; }

    struct RollRect { int lane; int startStep; int lengthSteps; };
    void setRolls (const std::vector<RollRect>& rollsToDraw);

    /** Fired on mouse-up: (lane, startStep, lengthSteps, endDensity 0..1). */
    std::function<void (int, int, int, float)> onRollPainted;

    /** Optional: while painting, the owner returns how many hits the roll currently
        under the brush would compile to (via RollCompiler), for the live meter.
        Keeping the count here rather than the compiler keeps this overlay model-free. */
    std::function<int (int lane, int startStep, int lengthSteps, float density)> getHitCount;

    // Claim the step area ONLY while the brush is on. The `brushEnabled` term is not
    // redundant with setInterceptsMouseClicks(): JUCE honours that flag inside the DEFAULT
    // Component::hitTest, so an override that ignores it silently re-intercepts every click
    // — which is what stopped the grid's own steps from toggling while the brush was off.
    // Never claim the lane-header column either: labels and lock padlocks live there and
    // belong to the grid beneath.
    bool hitTest (int x, int /*y*/) override { return brushEnabled && x >= labelWidth; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void paint     (juce::Graphics&) override;

private:
    int laneAt (int y) const noexcept;
    int stepAt (int x) const noexcept;
    juce::Rectangle<int> cellRect (int lane, int startStep, int len) const noexcept;
    void updatePreview() noexcept;   // recompute previewHits from the current drag

    const int numLanes;
    const int numSteps;
    const int labelWidth;

    bool  brushEnabled  = false;
    bool  dragging      = false;
    int   dragLane      = 0;
    int   dragStartStep = 0;
    int   dragCurStep   = 0;
    int   dragStartY    = 0;
    float dragDensity   = 0.5f;
    int   previewHits   = 0;   // hits the current stroke would produce (via getHitCount)

    std::vector<RollRect> rolls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollBrushOverlay)
};

} // namespace rollforge

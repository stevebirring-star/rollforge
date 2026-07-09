#pragma once

// RollForge — PadInspector: the per-pad TONE + SEND controls, shown in a CallOutBox
// when you right-click a pad.
//
// They live here rather than on the pad face because a pad already carries M, S, R,
// two trim handles, a waveform and a meter; two more inline controls would make an
// 80-pixel tile unusable. A right-click bubble keeps the grid readable and still puts
// the controls on the pad you clicked.
//
// SIMILAR walks this pad through the sounds in the library that are most like the one it
// holds — same drum type, nearest in feature space. Press it again and it steps to the
// next-nearest, so auditioning a shortlist is one repeated click, not a trip to a browser.

#include "engine/EngineCommand.h"   // LayerMode

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace rollforge
{

class PadInspector final : public juce::Component
{
public:
    PadInspector (const juce::String& padName, float tone, float reverbSend,
                  int numLayers, LayerMode layerMode, bool canFindSimilar);

    /** Fired live as the knobs move (the engine applies them without retriggering). */
    std::function<void (float tone)> onToneChanged;
    std::function<void (float send)> onSendChanged;
    std::function<void (LayerMode)>  onLayerModeChanged;

    /** Fired by SIMILAR. Swap this pad to the next-nearest library sound and return its
        name. An empty return means the library holds nothing else of this drum type; the
        button then retires itself and says so, rather than doing nothing on every press. */
    std::function<juce::String()> onSimilar;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void findSimilar();

    juce::Label  title;
    juce::Slider toneSlider;
    juce::Slider sendSlider;
    juce::Label    toneCaption;
    juce::Label    sendCaption;
    juce::Label    layersCaption;   // "N layers" — hidden on a single-sample pad
    juce::ComboBox layerModeBox;    // round-robin vs velocity
    juce::TextButton similarButton { "Similar" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadInspector)
};

} // namespace rollforge

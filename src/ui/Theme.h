#pragma once

// RollForge — Theme: every colour the UI uses, in one place, behind named ROLES.
//
// Before this, twelve files carried hex literals and the same grey appeared under four
// different values. A role ("the colour of a raised panel's top edge") survives a repaint
// of the whole app; a hex does not.
//
// Colour here is meaning, not decoration. The rules the rest of the UI must keep:
//
//   * accentHot  is USER INTENT — generate, arm, engage. Make a Beat, Reroll, Vary, the
//                triplet toggle, the roll brush. If the user did it, it glows hot.
//   * ember      is A VALUE — knob arcs, fader fills. Warm, but it is not an action; it is
//                where a control currently sits. At rest it is the only bright thing.
//   * accentCool is THE MACHINE RUNNING — transport, playhead, meters, the VU glass.
//                Never use it for a button the user presses to change something.
//   * category   is WHAT A SOUND IS — a kick is always the same colour, on its pad, on
//                its sequencer lane, on its meter. This is the single biggest reason
//                Battery and Maschine read as instruments and a grey grid does not.
//
// accentHot and accentCool must never sit adjacent at full saturation; put a panel or a
// hairline between them. That vibration is what makes an amateur UI hurt to look at.
//
// UI LAYER: this header is the only place a raw colour literal may appear.

#include "library/Categoriser.h"   // SoundCategory

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace rollforge
{

struct Theme
{
    const char* name = "";

    // --- Surfaces -----------------------------------------------------------
    juce::Colour background;      // the window behind everything
    juce::Colour backgroundDeep;  // the far end of the window's vertical gradient
    juce::Colour panel;           // a recessed panel (the sequencer well)
    juce::Colour panelRaised;     // a raised panel (the transport, the master strip)
    juce::Colour panelHighlight;  // 1px top edge of a raised panel: catches the light
    juce::Colour panelShadow;     // 1px bottom edge: sits in it
    juce::Colour hairline;        // the thin rule between sections

    // --- Text ---------------------------------------------------------------
    juce::Colour text;            // primary
    juce::Colour textDim;         // captions, units, secondary
    juce::Colour textFaint;       // disabled, out-of-range steps

    // --- Meaning ------------------------------------------------------------
    juce::Colour accentHot;       // user intent: generate, arm, engage
    juce::Colour accentHotDim;    // the same, at rest
    juce::Colour ember;           // a VALUE: knob arcs, fader fills. Warm but not an action.
    juce::Colour emberDim;
    juce::Colour accentCool;      // the machine running: transport, playhead, meters
    juce::Colour accentCoolDim;

    // --- Controls -----------------------------------------------------------
    juce::Colour knobBody;        // the machined cap
    juce::Colour knobBodyLight;   // its specular side
    juce::Colour knobSkirt;       // the ring it sits in
    juce::Colour knobPointer;     // the indicator line
    juce::Colour knobArc;         // the unfilled value arc
    juce::Colour sliderTrack;
    juce::Colour sliderFill;
    juce::Colour buttonFace;
    juce::Colour buttonFaceHover;

    // --- Meters -------------------------------------------------------------
    juce::Colour meterGlass;      // the backlit VU face
    juce::Colour meterGlassEdge;  // its vignette
    juce::Colour meterScale;      // printed scale + ticks
    juce::Colour meterNeedle;
    juce::Colour meterRedZone;    // above 0 VU
    juce::Colour meterPeakLamp;   // the clip lamp, lit

    // --- Sound categories ---------------------------------------------------
    // Indexed by SoundCategory. Chosen to stay distinguishable at 80 px, and to differ
    // in LIGHTNESS as well as hue so red/green colour blindness still separates them.
    std::array<juce::Colour, 9> category;

    juce::Colour colourFor (SoundCategory c) const noexcept
    {
        const auto i = (std::size_t) c;
        return i < category.size() ? category[i] : category[category.size() - 1];
    }
};

/** The palettes the app ships with. The owner picks one; everything else follows. */
enum class ThemeId
{
    forge = 0,      // forged steel, hot metal, blue meter glass
    studio,         // console grey, oxide red, amber lamps
    midnight,       // deep indigo, ice cyan, magenta
};

/** The active theme. Message-thread only; set once at startup. */
const Theme& theme() noexcept;
void setTheme (ThemeId id) noexcept;

/** Parses a theme name from settings ("forge"/"studio"/"midnight"); unknown -> forge. */
ThemeId themeIdFromString (const juce::String& name) noexcept;
const char* themeIdToString (ThemeId id) noexcept;

} // namespace rollforge

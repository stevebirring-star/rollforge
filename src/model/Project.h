#pragma once

// RollForge — Project: the serialisable session (Phase 6). Aggregates the pad
// sample paths + params, the pattern, the master-FX macro values, and transport.
// The app populates it on save and applies it on load; ProjectIO turns it into a
// `.rollforge` JSON file. Pure model (juce_core only).

#include "model/Pattern.h"
#include "model/PatternBank.h"

#include <juce_core/juce_core.h>

#include <array>

namespace rollforge
{

inline constexpr int projectNumPads = 16;

struct ProjectPad
{
    juce::String samplePath;             // layer 0's path ("" = the built-in starter sound)

    // Layers 1..N of a round-robin / velocity-layered pad, in order. Written under a
    // separate JSON key so a project saved before layers existed still loads: an absent
    // key simply means a one-layer pad, which is what every old project is.
    juce::StringArray extraLayerPaths;
    int          layerMode      = 0;      // LayerMode: 0 = round-robin, 1 = velocity

    float        gain           = 1.0f;
    float        pitchSemitones = 0.0f;
    float        pan            = 0.0f;
    int          chokeGroup     = 0;
    bool         reverse        = false;
    bool         muted          = false;
    bool         soloed         = false;
    float        startFraction  = 0.0f;   // sample trim start [0..1)
    float        endFraction    = 1.0f;   // sample trim end
    float        tone           = 0.0f;   // bipolar tilt EQ (-1 dark .. +1 bright)
    float        reverbSend     = 0.0f;   // 0..1 into the reverb send bus
};

struct Project
{
    int    version = 1;

    std::array<ProjectPad, projectNumPads> pads {};

    // `pattern` is the live one. It is also slots[currentSlot], and is written to the file
    // separately so that a build predating the A..H bank still loads the right groove out
    // of a newer file rather than an empty one.
    Pattern pattern;
    std::array<Pattern, numPatternSlots> slots {};
    int     currentSlot = 0;

    float  punch = 0.0f, space = 0.0f, crush = 0.0f, drive = 0.0f;  // master-FX macros
    float  lowEq = 0.0f, midEq = 0.0f, highEq = 0.0f;              // master EQ (dB per band)
    float  comp  = 0.0f;                                            // glue-compressor amount
    double bpm   = 120.0;
    float  swing = 0.0f;
};

} // namespace rollforge

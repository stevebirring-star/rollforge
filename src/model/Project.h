#pragma once

// RollForge — Project: the serialisable session (Phase 6). Aggregates the pad
// sample paths + params, the pattern, the master-FX macro values, and transport.
// The app populates it on save and applies it on load; ProjectIO turns it into a
// `.rollforge` JSON file. Pure model (juce_core only).

#include "model/Pattern.h"
#include "model/PatternBank.h"
#include "model/Song.h"

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

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

    // Heap, not std::array. A Pattern is ~52 KB, so eight of them by value put Project at
    // 473 KB — and a Project is a stack object: captureProject() returns one, fromJson()
    // builds a temporary, a test declares two. That overflowed the stack under ASan, and
    // Windows gives a thread 1 MB by default. Always `numPatternSlots` long.
    std::vector<Pattern> slots = std::vector<Pattern> ((std::size_t) numPatternSlots);
    int     currentSlot = 0;

    Song    song;             // the arrangement chain over those slots
    bool    songMode = false; // whether the chain, rather than one slot, is driving

    float  punch = 0.0f, space = 0.0f, crush = 0.0f, drive = 0.0f;  // master-FX macros
    float  lowEq = 0.0f, midEq = 0.0f, highEq = 0.0f;              // master EQ (dB per band)
    float  comp  = 0.0f;                                            // glue-compressor amount
    double bpm   = 120.0;
    float  swing = 0.0f;
};

// The guard for the reasoning above: keep Project off the stack's danger list.
static_assert (sizeof (Project) < 128 * 1024,
               "Project is built on the stack; hold something this big by pointer instead");

} // namespace rollforge

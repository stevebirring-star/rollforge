#pragma once

// RollForge — Project: the serialisable session (Phase 6). Aggregates the pad
// sample paths + params, the pattern, the master-FX macro values, and transport.
// The app populates it on save and applies it on load; ProjectIO turns it into a
// `.rollforge` JSON file. Pure model (juce_core only).

#include "model/Pattern.h"

#include <juce_core/juce_core.h>

#include <array>

namespace rollforge
{

inline constexpr int projectNumPads = 16;

struct ProjectPad
{
    juce::String samplePath;             // relative or absolute path to the sample
    float        gain           = 1.0f;
    float        pitchSemitones = 0.0f;
    float        pan            = 0.0f;
    int          chokeGroup     = 0;
    bool         reverse        = false;
    bool         muted          = false;
    bool         soloed         = false;
};

struct Project
{
    int    version = 1;

    std::array<ProjectPad, projectNumPads> pads {};
    Pattern pattern;

    float  punch = 0.0f, space = 0.0f, crush = 0.0f, drive = 0.0f;  // master-FX macros
    float  lowEq = 0.0f, midEq = 0.0f, highEq = 0.0f;              // master EQ (dB per band)
    float  comp  = 0.0f;                                            // glue-compressor amount
    double bpm   = 120.0;
    float  swing = 0.0f;
};

} // namespace rollforge

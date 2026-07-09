#pragma once

// RollForge — BeatboxDetector: a mouth, a microphone, a pattern.
//
// Finds the hits in a recording of someone beatboxing and decides, for each one, whether it
// was a kick, a snare or a hat. The result is a list of Capture::Hit, which is exactly what
// tapping the pads produces — so from there on the two are the same feature.
//
// THREE CLASSES, NOT EIGHT. A human mouth makes roughly three drum sounds: a boomy "b", a
// noisy "k"/"p", and a bright "ts". Offering a tom and an open hat would be a classifier
// pretending to a resolution its input does not have. Categoriser's eight categories are
// collapsed onto three pads on the way out.
//
// HONEST LIMIT. This is onset detection plus the same coarse time-domain features that
// categorise the sample library: brightness, decay, transient count. It tells a "b" from a
// "ts" reliably and a soft "p" from a "b" less so. It is a sketchpad, not a transcription
// system, and a bad take is a bad take -- the fix is to record it again, not to argue with it.
//
// Pure: no GUI, no audio thread, no allocation anywhere near real time.
//
// LIBRARY LAYER: juce_core, no GUI.

#include "library/Slicer.h"
#include "model/Capture.h"

#include <vector>

namespace rollforge
{
namespace BeatboxDetector
{
    struct Options
    {
        // Where the three classes land. Defaults match the standard pad layout: kick, snare,
        // closed hat.
        int kickPad  = 0;
        int snarePad = 1;
        int hatPad   = 2;

        /** How much audio after an onset is used to judge it. Long enough to hear a kick's
            body, short enough not to swallow the next hit. */
        double judgeSeconds = 0.12;

        /** An "onset" whose window peaks below this fraction of the loudest hit is silence:
            a breath, a chair creak, a lip smack before the take begins. */
        float silenceFraction = 0.06f;

        /** Where a hit is judged to have ended, as a fraction of its own peak. The judging
            window is trimmed to this before the features are measured, because padding a 35 ms
            "ts" out to 120 ms with silence drags its zero-crossing rate down into snare
            territory -- the detector then hears a snare where a person heard a hat. */
        float tailFraction = 0.05f;

        // Timing, not tiling: never snap the first onset to sample 0.
        Slicer::Options onsets { .snapFirstToZero = false };
    };

    /** Hits, timed in seconds from sample 0 of `mono`, in ascending time order. Safe on null,
        empty, or silent input (returns empty). */
    std::vector<Capture::Hit> detect (const float* mono, int numSamples, double sampleRate,
                                      const Options& options = {});
}
} // namespace rollforge

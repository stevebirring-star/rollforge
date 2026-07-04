#pragma once

// RollForge — MidiExporter: turns a Pattern into a MIDI drum performance. Each on
// step becomes a note on GM channel 10 at its pad's GM drum note; ratchets and
// compiled rolls are flattened into individual notes; forward micro-shift is
// applied. (Swing / probability / the Humaniser are live-playback grooves and are
// NOT baked into the export.) Pure model (juce_audio_basics), headless-testable.

#include "model/Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{
namespace MidiExporter
{
    /** The GM drum note for a pad (0..15). */
    int gmNoteForPad (int pad) noexcept;

    /** Builds a note sequence for `bars` bars of the pattern (16 steps/bar). */
    juce::MidiMessageSequence toSequence (const Pattern& pattern, int bars = 1, int ticksPerQuarter = 960);

    /** Writes a MIDI file (tempo track + the drum track). */
    bool save (const Pattern& pattern, const juce::File& file, int bars = 1);
}
} // namespace rollforge

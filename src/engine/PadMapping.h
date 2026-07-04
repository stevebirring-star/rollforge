#pragma once

// RollForge — PadMapping: pure functions mapping external inputs to pad indices.
//
// Zero dependencies (no JUCE), so it is trivially unit-testable. The trigger
// wiring (MIDI callback, keyboard handler) lives elsewhere; this is just the
// policy of "which input maps to which pad".

namespace rollforge
{

// MIDI notes 36..51 (GM kick upward) map linearly to pads 0..15.
inline constexpr int midiBaseNote     = 36;
inline constexpr int numMappablePads  = 16;

/** MIDI note number -> pad index (0..15), or -1 if outside the mapped range. */
int midiNoteToPad (int noteNumber);

/** Lowercase computer-keyboard character -> pad index (0..15), or -1 if
    unmapped. The layout mirrors the 4x4 grid, row 0 at the top:
        1 2 3 4      (pads 0..3)
        q w e r      (pads 4..7)
        a s d f      (pads 8..11)
        z x c v      (pads 12..15) */
int keyCharToPad (int lowercaseChar);

} // namespace rollforge

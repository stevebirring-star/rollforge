#pragma once

// RollForge — EngineCommand: a trivially-copyable message sent from a producer
// thread (UI / keyboard / MIDI) to the audio thread through a lock-free
// CommandQueue. See CommandQueue.h for the transport and threading rules.
//
// RT-SAFETY: command payloads MUST stay trivially-copyable POD — no strings, no
// smart pointers, no heap — so pushing/draining them on the audio thread never
// allocates or runs non-trivial code. The static_assert below enforces this.
// A setPad command therefore carries a RAW SampleBuffer* (a pointer is trivially
// copyable): the PRODUCER guarantees the buffer stays alive until the audio
// thread has adopted it, and — if replacing a live pad sample — retires the old
// buffer first (see SampleBuffer.h).
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/VoiceParameters.h"

#include <cstdint>
#include <type_traits>

namespace rollforge
{

class SampleBuffer;   // forward-declared: setPad carries a non-owning raw pointer

enum class CommandType : std::uint8_t
{
    none = 0,
    triggerPad,     // play padIndex at the given velocity
    setPad,         // (re)configure padIndex: sample + params + choke group
};

struct EngineCommand
{
    CommandType type     = CommandType::none;
    int         padIndex = 0;
    float       velocity = 1.0f;      // triggerPad: normalised 0..1

    // setPad payload (ignored for triggerPad):
    SampleBuffer*   sample = nullptr; // non-owning; producer guarantees lifetime
    VoiceParameters params {};
    int             chokeGroup = 0;

    static EngineCommand makeTrigger (int padIndex, float velocity) noexcept
    {
        EngineCommand c;
        c.type     = CommandType::triggerPad;
        c.padIndex = padIndex;
        c.velocity = velocity;
        return c;
    }

    static EngineCommand makeSetPad (int padIndex,
                                     SampleBuffer* sample,
                                     const VoiceParameters& params,
                                     int chokeGroup) noexcept
    {
        EngineCommand c;
        c.type       = CommandType::setPad;
        c.padIndex   = padIndex;
        c.sample     = sample;
        c.params     = params;
        c.chokeGroup = chokeGroup;
        return c;
    }
};

static_assert (std::is_trivially_copyable_v<EngineCommand>,
               "EngineCommand must stay trivially copyable — it is copied across "
               "the lock-free audio-thread FIFO. No strings / smart pointers / heap.");

} // namespace rollforge

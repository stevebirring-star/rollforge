#pragma once

// RollForge — EngineCommand: a trivially-copyable message sent from a producer
// thread (UI / keyboard / MIDI) to the audio thread through a lock-free
// CommandQueue. See CommandQueue.h for the transport and threading rules.
//
// RT-SAFETY: command payloads MUST stay trivially-copyable POD — no strings, no
// smart pointers, no heap — so pushing/draining them on the audio thread never
// allocates or runs non-trivial code. The static_assert below enforces this; if
// you need to attach a sample to a command later, pass a raw SampleBuffer* whose
// lifetime the message-thread retirement set guarantees (see SampleBuffer.h),
// never a Ptr.
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include <cstdint>
#include <type_traits>

namespace rollforge
{

enum class CommandType : std::uint8_t
{
    none = 0,
    triggerPad,     // play padIndex at the given velocity
};

struct EngineCommand
{
    CommandType type     = CommandType::none;
    int         padIndex = 0;
    float       velocity = 1.0f;   // normalised 0..1

    static EngineCommand makeTrigger (int padIndex, float velocity) noexcept
    {
        EngineCommand c;
        c.type     = CommandType::triggerPad;
        c.padIndex = padIndex;
        c.velocity = velocity;
        return c;
    }
};

static_assert (std::is_trivially_copyable_v<EngineCommand>,
               "EngineCommand must stay trivially copyable — it is copied across "
               "the lock-free audio-thread FIFO. No strings / smart pointers / heap.");

} // namespace rollforge

#pragma once

// RollForge — EngineCommand: a trivially-copyable message sent from a producer
// thread (UI / keyboard / MIDI) to the audio thread through a lock-free
// CommandQueue. See CommandQueue.h for the transport and threading rules.
//
// RT-SAFETY: command payloads MUST stay trivially-copyable POD — no strings, no
// smart pointers, no heap — so pushing/draining them on the audio thread never
// allocates or runs non-trivial code. The static_assert below enforces this.
// A setPad command therefore carries RAW SampleBuffer*s (pointers are trivially
// copyable): the PRODUCER guarantees the buffers stay alive until the audio
// thread has adopted them, and — if replacing a live pad sample — retires the old
// buffers first (see SampleBuffer.h).
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/VoiceParameters.h"

#include <cstdint>
#include <type_traits>

namespace rollforge
{

class SampleBuffer;   // forward-declared: setPad carries non-owning raw pointers

/** How many samples a pad can hold. Must equal model/Pad.h's maxSampleAlternates
    (KitInstaller static_asserts it); the engine can't include the model layer. */
inline constexpr int maxPadLayers = 4;

/** How a pad chooses among its layers when a hit arrives. */
enum class LayerMode : int
{
    roundRobin = 0,   // cycle through them, so machine-gun repeats don't sound machine-gunned
    velocity   = 1,   // soft hits pick the early layers, hard hits the late ones
};

enum class CommandType : std::uint8_t
{
    none = 0,
    triggerPad,     // play padIndex at the given velocity
    setPad,         // (re)configure padIndex: layers + params + choke group
};

struct EngineCommand
{
    CommandType type     = CommandType::none;
    int         padIndex = 0;
    float       velocity = 1.0f;      // triggerPad: normalised 0..1

    // setPad payload (ignored for triggerPad). layers[0] is the primary sample; a pad
    // with numLayers > 1 round-robins or velocity-switches between them.
    SampleBuffer*   layers[maxPadLayers] {};   // non-owning; producer guarantees lifetime
    int             numLayers  = 0;
    int             layerMode  = (int) LayerMode::roundRobin;
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
                                     SampleBuffer* const* layers,
                                     int numLayers,
                                     int layerMode,
                                     const VoiceParameters& params,
                                     int chokeGroup) noexcept
    {
        EngineCommand c;
        c.type       = CommandType::setPad;
        c.padIndex   = padIndex;
        c.numLayers  = numLayers < 0 ? 0 : (numLayers > maxPadLayers ? maxPadLayers : numLayers);
        c.layerMode  = layerMode;
        c.params     = params;
        c.chokeGroup = chokeGroup;
        for (int i = 0; i < c.numLayers; ++i)
            c.layers[i] = layers[i];
        return c;
    }
};

static_assert (std::is_trivially_copyable_v<EngineCommand>,
               "EngineCommand must stay trivially copyable — it is copied across "
               "the lock-free audio-thread FIFO. No strings / smart pointers / heap.");

} // namespace rollforge

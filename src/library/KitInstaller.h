#pragma once

// RollForge — KitInstaller: bridges the model Kit onto the audio-thread DrumEngine.
//
// This is where the model layer (Pad/Kit) meets the engine layer (DrumEngine):
// it maps each Pad's parameters onto VoiceParameters and hands the pad's sample +
// choke group to the engine through the lock-free command queue (pushSetPad).
//
// LIFETIME: `kit` must stay alive until the engine has drained the setPad commands
// (its next process()), after which the DrumEngine holds its own references. The
// caller (e.g. the app) typically keeps the Kit for the whole session.
//
// LIBRARY LAYER: no JUCE GUI includes.

#include "engine/DrumEngine.h"
#include "engine/SampleRetirementPool.h"
#include "model/Kit.h"

namespace rollforge
{

/** Maps a model Pad's parameters onto engine VoiceParameters. */
VoiceParameters toVoiceParameters (const Pad& pad);

/** Enqueues a setPad for every pad in `kit` (sample + params + choke group). */
void installKitIntoEngine (const Kit& kit, DrumEngine& engine);

/** Replaces pad `padIndex`'s sample with `newSample`: retires the old buffer for
    RT-safe reclamation (so any voice still playing it is never freed on the audio
    thread), updates the Kit, and installs the new sample + the pad's params/choke
    into the engine. Message thread only. No-op if the index is invalid or
    `newSample` is null. */
void installSampleIntoPad (SampleRetirementPool& retirementPool,
                           Kit& kit,
                           DrumEngine& engine,
                           int padIndex,
                           SampleBuffer::Ptr newSample);

/** Re-pushes pad `padIndex`'s current params + choke to the engine WITHOUT changing
    its sample — used for live per-pad edits (trim / reverse). No retirement, since
    the sample buffer is unchanged. Message thread only. */
void updatePadParamsInEngine (const Kit& kit, DrumEngine& engine, int padIndex);

} // namespace rollforge

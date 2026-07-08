#pragma once

// RollForge — MasterBus: the master output chain applied after the Sequencer mix.
// Chain: the four macro FX (Punch / Space / Crush / Drive) -> a 3-band EQ + a glue
// compressor -> the always-on MasterLimiter. RT-safe; all effects flat/0 = a clean
// pass through (plus limiting).
//
// ENGINE LAYER RULE: no JUCE GUI includes.

#include "engine/fx/Compressor.h"
#include "engine/fx/Crush.h"
#include "engine/fx/Drive.h"
#include "engine/fx/MasterEq.h"
#include "engine/fx/MasterLimiter.h"
#include "engine/fx/Punch.h"
#include "engine/fx/Space.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace rollforge
{

class MasterBus
{
public:
    void prepare (double sampleRate, int blockSize) noexcept;
    void reset() noexcept;

    /** Processes the master mix in place: macro FX chain, then the limiter. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // Macro controls (0..1, 0 = bypass). Message-thread safe.
    void setDrive (float amount) noexcept { drive.setAmount (amount); }
    float getDrive() const noexcept { return drive.getAmount(); }
    void setCrush (float amount) noexcept { crush.setAmount (amount); }
    float getCrush() const noexcept { return crush.getAmount(); }
    void setPunch (float amount) noexcept { punch.setAmount (amount); }
    float getPunch() const noexcept { return punch.getAmount(); }
    void setSpace (float amount) noexcept { space.setAmount (amount); }
    float getSpace() const noexcept { return space.getAmount(); }

    // Master EQ (dB per band) + one-knob glue compressor (0..1). Message-thread safe.
    void  setLowEqDb  (float db) noexcept { eq.setLowDb (db); }
    void  setMidEqDb  (float db) noexcept { eq.setMidDb (db); }
    void  setHighEqDb (float db) noexcept { eq.setHighDb (db); }
    float getLowEqDb()  const noexcept { return eq.getLowDb(); }
    float getMidEqDb()  const noexcept { return eq.getMidDb(); }
    float getHighEqDb() const noexcept { return eq.getHighDb(); }
    void  setComp (float amount) noexcept { comp.setAmount (amount); }
    float getComp() const noexcept { return comp.getAmount(); }

    MasterLimiter& getLimiter() noexcept { return limiter; }

private:
    Punch         punch;
    Drive         drive;
    Crush         crush;
    Space         space;
    MasterEq      eq;
    Compressor    comp;
    MasterLimiter limiter;
};

} // namespace rollforge

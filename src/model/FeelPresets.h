#pragma once

// RollForge — FeelPresets: named "feel" settings that drive the Humaniser + swing
// together, so a generated beat never sounds quantized-robotic. Exposes the
// one-knob Humaniser as musician-friendly names (Straight / Human / Boom-bap loose
// / Trap tight / Drunk) instead of a bare 0..1 number. Pure (no JUCE).

namespace rollforge
{
namespace FeelPresets
{
    enum Feel
    {
        Straight = 0,   // dead quantized
        Human,          // subtle, natural
        BoomBapLoose,   // laid-back + heavy swing
        TrapTight,      // tight, minimal drift
        Drunk,          // maximally loose
        NumFeels
    };

    const char* name (Feel feel) noexcept;

    struct Settings
    {
        float humanise;   // Humaniser amount 0..1 (timing + velocity jitter)
        float swing;      // swing 0..1 (delay on the off-8ths)
    };

    Settings settingsFor (Feel feel) noexcept;
}
} // namespace rollforge

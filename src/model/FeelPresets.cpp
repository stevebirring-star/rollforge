#include "model/FeelPresets.h"

namespace rollforge
{
namespace FeelPresets
{

const char* name (Feel feel) noexcept
{
    switch (feel)
    {
        case Straight:     return "Straight";
        case Human:        return "Human";
        case BoomBapLoose: return "Boom-bap loose";
        case TrapTight:    return "Trap tight";
        case Drunk:        return "Drunk";
        default:           return "Feel";
    }
}

Settings settingsFor (Feel feel) noexcept
{
    switch (feel)
    {
        case Straight:     return { 0.00f, 0.00f };
        case Human:        return { 0.40f, 0.12f };
        case BoomBapLoose: return { 0.60f, 0.55f };
        case TrapTight:    return { 0.18f, 0.08f };
        case Drunk:        return { 0.90f, 0.25f };
        default:           return { 0.00f, 0.00f };
    }
}

} // namespace FeelPresets
} // namespace rollforge

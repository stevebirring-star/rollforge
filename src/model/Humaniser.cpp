#include "model/Humaniser.h"

namespace rollforge
{
namespace Humaniser
{

namespace
{
    std::uint64_t mix (std::uint64_t x) noexcept
    {
        x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
        x ^= x >> 27; x *= 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    float unitFromHash (std::uint64_t h) noexcept
    {
        return (float) ((double) (h >> 40) / (double) (1u << 24));   // [0, 1)
    }

    float clampf (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
}

std::uint64_t eventHash (std::int64_t stepIndex, int lane, int salt) noexcept
{
    return mix ((std::uint64_t) stepIndex * 0x9E3779B97F4A7C15ull
              + (std::uint64_t) (lane + 1) * 0xC2B2AE3D27D4EB4Full
              + (std::uint64_t) (salt + 1) * 0x165667B19E3779F9ull);
}

float timingSteps (std::uint64_t eventHash, float amount) noexcept
{
    amount = clampf (amount, 0.0f, 1.0f);
    return amount * 0.2f * unitFromHash (eventHash);   // [0, 0.2*amount] steps, forward-only
}

float velocityDelta (std::uint64_t eventHash, float amount) noexcept
{
    amount = clampf (amount, 0.0f, 1.0f);
    const float signed01 = unitFromHash (eventHash) * 2.0f - 1.0f;   // [-1, 1)
    return amount * 0.25f * signed01;                                // +/- 0.25*amount
}

} // namespace Humaniser
} // namespace rollforge

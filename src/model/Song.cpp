#include "model/Song.h"

namespace rollforge
{

namespace
{
    int barsOf (const SongStep& step) noexcept { return step.bars < 1 ? 1 : step.bars; }
}

int Song::totalBars() const noexcept
{
    int total = 0;
    for (const auto& step : steps)
        total += barsOf (step);
    return total;
}

int Song::stepAtBar (int bar) const noexcept
{
    if (steps.empty() || bar < 0)
        return -1;

    const int total = totalBars();
    if (total <= 0)
        return -1;   // unreachable while barsOf() >= 1, but the modulo below must never divide by 0

    if (bar >= total)
    {
        if (! loop)
            return -1;
        bar %= total;
    }

    for (int i = 0; i < (int) steps.size(); ++i)
    {
        const int length = barsOf (steps[(std::size_t) i]);
        if (bar < length)
            return i;
        bar -= length;
    }

    return -1;   // unreachable: the loop consumes exactly `total` bars
}

int Song::slotAtBar (int bar) const noexcept
{
    const int index = stepAtBar (bar);
    if (index < 0)
        return -1;

    const int slot = steps[(std::size_t) index].slot;
    return PatternBank::isValidSlot (slot) ? slot : -1;
}

} // namespace rollforge

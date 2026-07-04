#include "engine/SampleRetirementPool.h"

namespace rollforge
{

void SampleRetirementPool::retire (SampleBuffer::Ptr buffer)
{
    if (buffer != nullptr)
        retired.addIfNotAlreadyThere (buffer);
}

int SampleRetirementPool::sweep()
{
    int freed = 0;

    // Iterate backwards so removals don't disturb the indices still to visit.
    for (int i = retired.size(); --i >= 0;)
    {
        // Inspect via the RAW pointer: taking a temporary Ptr here would add a
        // reference and mask the "no Voice references it" condition we test for.
        if (retired.getObjectPointer (i)->getReferenceCount() == 1)
        {
            retired.remove (i);   // drops the pool's last ref -> delete, here on
            ++freed;              // the message thread (never the audio thread).
        }
    }

    return freed;
}

} // namespace rollforge

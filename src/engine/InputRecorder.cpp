#include "engine/InputRecorder.h"

#include <algorithm>
#include <cmath>

namespace rollforge
{

void InputRecorder::prepare (double newSampleRate, double maxSeconds)
{
    stop();

    // Wait for any block still inside the buffer before we resize it under its feet.
    while (writing.load (std::memory_order_acquire))
        juce::Thread::yield();

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    // Only nonsense is corrected. Rounding a short request UP to a second would hand the
    // caller a buffer bigger than the one they asked for and let a take run past its own end.
    const double seconds = maxSeconds > 0.0 ? maxSeconds : 30.0;
    const auto capacity = (std::size_t) std::max (1.0, std::ceil (sampleRate * seconds));
    buffer.assign (capacity, 0.0f);

    written.store (0, std::memory_order_release);
    full.store (false, std::memory_order_release);
    startTransportSample.store (-1, std::memory_order_release);
}

void InputRecorder::start() noexcept
{
    written.store (0, std::memory_order_release);
    full.store (false, std::memory_order_release);
    startTransportSample.store (-1, std::memory_order_release);
    armed.store (true, std::memory_order_release);
}

void InputRecorder::stop() noexcept
{
    armed.store (false, std::memory_order_release);
}

void InputRecorder::writeBlock (const float* const* input, int numChannels, int numSamples,
                                std::int64_t transportSampleAtBlockStart) noexcept
{
    if (! armed.load (std::memory_order_acquire))
        return;
    if (input == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    writing.store (true, std::memory_order_release);

    // Re-check under the flag: stop() may have landed between the check above and here, and
    // take() must never read a buffer this block is still appending to.
    if (armed.load (std::memory_order_acquire))
    {
        if (startTransportSample.load (std::memory_order_acquire) < 0)
            startTransportSample.store (transportSampleAtBlockStart, std::memory_order_release);

        int at = written.load (std::memory_order_acquire);
        const int capacity = (int) buffer.size();
        const int room = capacity - at;

        if (room <= 0)
        {
            full.store (true, std::memory_order_release);
            armed.store (false, std::memory_order_release);
        }
        else
        {
            const int n = std::min (numSamples, room);
            const float scale = 1.0f / (float) numChannels;

            for (int i = 0; i < n; ++i)
            {
                float sum = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                    if (input[c] != nullptr)
                        sum += input[c][i];
                buffer[(std::size_t) (at + i)] = sum * scale;
            }

            at += n;
            written.store (at, std::memory_order_release);

            if (at >= capacity)
            {
                full.store (true, std::memory_order_release);
                armed.store (false, std::memory_order_release);
            }
        }
    }

    writing.store (false, std::memory_order_release);
}

std::vector<float> InputRecorder::take()
{
    armed.store (false, std::memory_order_release);

    // At most one audio block long. The acquire pairs with the release above, which is what
    // publishes the samples this thread is about to read.
    while (writing.load (std::memory_order_acquire))
        juce::Thread::yield();

    const int n = written.load (std::memory_order_acquire);
    std::vector<float> out (buffer.begin(), buffer.begin() + (std::ptrdiff_t) std::max (0, n));

    written.store (0, std::memory_order_release);
    full.store (false, std::memory_order_release);
    return out;
}

} // namespace rollforge

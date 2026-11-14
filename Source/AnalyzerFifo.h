#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/** Lock-free single-producer / single-consumer sample queue from the audio
    thread to the display. Stores a mono mix (L+R)/2. If the display falls
    behind, new samples are dropped rather than blocking the audio thread. */
class AnalyzerFifo
{
public:
    AnalyzerFifo() : buffer ((size_t) capacity, 0.0f) {}

    template <typename T>
    void push (const T* left, const T* right, int numSamples)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
        auto write = [&] (int start, int count, int offset)
        {
            for (int i = 0; i < count; ++i)
            {
                const T l = left[offset + i];
                const T r = right != nullptr ? right[offset + i] : l;
                buffer[(size_t) (start + i)] = (float) (0.5 * (double) (l + r));
            }
        };
        write (s1, n1, 0);
        write (s2, n2, n1);
        fifo.finishedWrite (n1 + n2);
    }

    int pull (float* dest, int maxSamples)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxSamples, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) dest[i] = buffer[(size_t) (s1 + i)];
        for (int i = 0; i < n2; ++i) dest[n1 + i] = buffer[(size_t) (s2 + i)];
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

private:
    static constexpr int capacity = 1 << 15;
    juce::AbstractFifo fifo { capacity };
    std::vector<float> buffer;
};

#pragma once

#include "dsp/FilterDesign.h"
#include <algorithm>
#include <vector>

namespace puzzleq {

// 2x oversampler: zero-stuff / decimate through a 8th-order Butterworth
// half-band. Removes Nyquist cramping so 10–16 kHz bells stay analog-shaped.
class Oversampler2x
{
public:
    void prepare (float sampleRate, int maxBlock)
    {
        sr = sampleRate;
        const float fc = sampleRate * 0.45f; // just below original Nyquist, at 2x rate
        designBand (FilterShape::HighCut, fc, 0.0f, 0.707f, 48.0f, false, sampleRate * 2.0f, false, upL);
        upR = upL; upR.reset();
        downL = upL; downL.reset();
        downR = upL; downR.reset();
        bufL.assign (static_cast<size_t> (std::max (maxBlock, 64) * 2), 0.0f);
        bufR.assign (static_cast<size_t> (std::max (maxBlock, 64) * 2), 0.0f);
    }

    void reset()
    {
        upL.reset();
        upR.reset();
        downL.reset();
        downR.reset();
    }

    int capacity() const noexcept { return static_cast<int> (bufL.size() / 2); }

    void upsample (const float* l, const float* r, int n)
    {
        // Never allocate on the audio thread. Caller must chunk to capacity().
        n = std::min (n, capacity());
        for (int i = 0; i < n; ++i)
        {
            bufL[static_cast<size_t> (i * 2)]     = upL.process (l[i] * 2.0f);
            bufL[static_cast<size_t> (i * 2 + 1)] = upL.process (0.0f);
            bufR[static_cast<size_t> (i * 2)]     = upR.process (r[i] * 2.0f);
            bufR[static_cast<size_t> (i * 2 + 1)] = upR.process (0.0f);
        }
    }

    void downsample (float* l, float* r, int n)
    {
        n = std::min (n, capacity());
        for (int i = 0; i < n; ++i)
        {
            const float aL = downL.process (bufL[static_cast<size_t> (i * 2)]);
            (void) downL.process (bufL[static_cast<size_t> (i * 2 + 1)]);
            const float aR = downR.process (bufR[static_cast<size_t> (i * 2)]);
            (void) downR.process (bufR[static_cast<size_t> (i * 2 + 1)]);
            l[i] = aL;
            r[i] = aR;
        }
    }

    float* left2() noexcept { return bufL.data(); }
    float* right2() noexcept { return bufR.data(); }
    int latency() const noexcept { return 8; }

private:
    float sr = 48000.0f;
    Cascade upL, upR, downL, downR;
    std::vector<float> bufL, bufR;
};

} // namespace puzzleq

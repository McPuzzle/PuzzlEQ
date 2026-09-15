#pragma once

#include "dsp/Fft.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace puzzleq {

class SpectrumAnalyzer
{
public:
    void prepare (float sampleRate, int fftSize = 4096);
    void reset();

    void push (const float* left, const float* right, int numSamples, bool pre);
    bool consume (std::vector<float>& magDbOut, bool pre);
    bool consumePeaks (std::vector<float>& peakDbOut, bool pre);

    void setTilt (float dbPerOct) noexcept { tiltDbOct = dbPerOct; }
    void setSmoothing (float s) noexcept { smoothing = s; }
    void setFrozen (bool f) noexcept { frozen = f; }

    float sampleRate() const noexcept { return sr; }
    int fftSize() const noexcept { return fft.size(); }

    static float binToHz (int bin, int fftSize, float sr) noexcept
    {
        return static_cast<float> (bin) * sr / static_cast<float> (fftSize);
    }

    static int hzToBin (float hz, int fftSize, float sr) noexcept
    {
        const int top = fftSize / 2;
        int b = static_cast<int> (std::lround (hz * static_cast<float> (fftSize) / sr));
        if (b < 0) b = 0;
        if (b > top) b = top;
        return b;
    }

    // Quadratic peak offset in bins. ym1/y0/yp1 are consecutive bin values.
    static float parabolicDelta (float ym1, float y0, float yp1) noexcept
    {
        const float d = ym1 - 2.0f * y0 + yp1;
        if (std::abs (d) < 1.0e-8f)
            return 0.0f;
        return 0.5f * (ym1 - yp1) / d;
    }

    bool copyCurrent (std::vector<float>& magDbOut, bool pre) const;
    bool copyPeaks (std::vector<float>& peakDbOut, bool pre) const;

private:
    struct Channel
    {
        std::vector<float> fifo;
        int fifoWrite = 0;
        int fifoFilled = 0;
        std::vector<float> magSmooth;
        std::vector<float> peakHold;
        std::atomic<bool> ready { false };
        mutable std::mutex mutex;
    };

    void processFifo (Channel& ch);

    Fft fft;
    float sr = 48000.0f;
    float tiltDbOct = 4.5f;
    float smoothing = 0.55f;
    bool frozen = false;
    Channel preCh, postCh;
    std::vector<float> windowed, re, im, hann;
};

} // namespace puzzleq

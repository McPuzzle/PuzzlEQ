#pragma once

#include "state/BandState.h"
#include "dsp/Fft.h"
#include "dsp/FilterDesign.h"
#include <vector>
#include <array>

namespace puzzleq {

int fftSizeForResolution (LinearResolution res) noexcept;
int latencyForResolution (LinearResolution res) noexcept;

class LinearPhaseEq
{
public:
    void prepare (float sampleRate, LinearResolution res);
    void reset();
    void setResolution (LinearResolution res);

    void updateFromBands (const std::array<BandState, kMaxBands>& bands,
                          float gainScale,
                          int soloBand,
                          bool naturalIgnored);

    void process (float* left, float* right, int numSamples);

    int latencySamples() const noexcept { return hop; }
    float magnitudeAt (float hz) const;
    int fftSize() const noexcept { return n; }

private:
    struct Channel
    {
        std::vector<float> hist;
        std::vector<float> ola;
        int histWrite = 0;
        int collected = 0;
        int olaPos = 0;
    };

    void rebuildResponse();
    void processChannel (Channel& ch, float* data, int numSamples);

    float sr = 48000.0f;
    LinearResolution resolution = LinearResolution::Medium;
    Fft fft;
    int n = 0;
    int hop = 0;
    std::vector<float> H, re, im, time, hann;
    Channel chL, chR;
    std::array<BandState, kMaxBands> lastBands {};
    float lastScale = 1.0f;
    int lastSolo = -1;
    bool dirty = true;
};

} // namespace puzzleq

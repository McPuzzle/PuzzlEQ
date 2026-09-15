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

    int latencySamples() const noexcept { return taps / 2; }

private:
    void rebuildIr();
    void processChannel (std::vector<float>& delay, int& write, float* data, int numSamples);

    float sr = 48000.0f;
    LinearResolution resolution = LinearResolution::Medium;
    Fft fft;
    int n = 0;
    int taps = 0;
    std::vector<float> ir, re, im;
    std::vector<float> delayL, delayR;
    int writeL = 0, writeR = 0;
    std::array<BandState, kMaxBands> lastBands {};
    float lastScale = 1.0f;
    int lastSolo = -1;
    bool dirty = true;
};

} // namespace puzzleq

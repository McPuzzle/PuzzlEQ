#pragma once

#include "state/BandState.h"
#include "dsp/Fft.h"
#include <array>
#include <vector>

namespace puzzleq {

class SpectralDynamics
{
public:
    void prepare (float sampleRate);
    void reset();
    bool hasWork (const std::array<BandState, kMaxBands>& bands) const noexcept;

    void process (float* left, float* right, int numSamples,
                  const std::array<BandState, kMaxBands>& bands,
                  float gainScale);

private:
    void processHop();

    float sr = 48000.0f;
    Fft fft;
    int n = 2048;
    int hop = 512;
    int collected = 0;
    std::vector<float> inL, inR, outL, outR, olaL, olaR;
    std::vector<float> time, re, im, hann;
    std::vector<float> env; // per-bin envelope
    int inWrite = 0;
    int outRead = 0;
    int olaPos = 0;
};

} // namespace puzzleq

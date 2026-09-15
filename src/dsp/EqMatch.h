#pragma once

#include "state/BandState.h"
#include <vector>
#include <array>

namespace puzzleq {

class EqMatch
{
public:
    void reset();
    void accumulate (const std::vector<float>& magDb, float sampleRate, int fftSize);
    void freezeAsReference();
    void freezeAsSource();
    bool hasSource() const noexcept { return sourceReady; }
    bool hasReference() const noexcept { return refReady; }

    // Fit parametric bells into destSlots (active bands written, others untouched).
    int fitBands (std::array<BandState, kMaxBands>& dest, int maxBands = 8) const;

    const std::vector<float>& source() const { return sourceDb; }
    const std::vector<float>& reference() const { return refDb; }

private:
    std::vector<float> accum, sourceDb, refDb;
    int frames = 0;
    float sr = 48000.0f;
    int fftN = 4096;
    bool sourceReady = false;
    bool refReady = false;
};

} // namespace puzzleq

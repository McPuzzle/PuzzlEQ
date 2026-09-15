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
    void setSourceFrom (const std::vector<float>& magDb, float sampleRate, int fftSize);
    void setReferenceFrom (const std::vector<float>& magDb, float sampleRate, int fftSize);
    bool hasSource() const noexcept { return sourceReady; }
    bool hasReference() const noexcept { return refReady; }

    // Fit parametric bands into dest (only empty slots are written).
    int fitBands (std::array<BandState, kMaxBands>& dest, int maxBands = 8) const;

    // Fit a sketched / drawn target curve (Hz, dB samples) into dest.
    int fitTargetCurve (const std::vector<float>& freqsHz,
                        const std::vector<float>& targetDb,
                        std::array<BandState, kMaxBands>& dest,
                        int maxBands = 8) const;

    const std::vector<float>& source() const { return sourceDb; }
    const std::vector<float>& reference() const { return refDb; }

private:
    int fitResidual (std::vector<float> residual,
                     float sampleRate,
                     int fftSize,
                     std::array<BandState, kMaxBands>& dest,
                     int maxBands) const;

    std::vector<float> accum, sourceDb, refDb;
    int frames = 0;
    float sr = 48000.0f;
    int fftN = 4096;
    bool sourceReady = false;
    bool refReady = false;
};

} // namespace puzzleq

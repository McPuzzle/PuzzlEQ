#pragma once

#include "state/BandState.h"
#include "dsp/FilterDesign.h"

namespace puzzleq {

class EnvelopeFollower
{
public:
    void prepare (float sampleRate) noexcept;
    void setTimes (float attackMs, float releaseMs) noexcept;
    float processPeak (float x) noexcept;
    void reset() noexcept { env = 0.0f; }

    float value() const noexcept { return env; }

private:
    float sr = 48000.0f;
    float atk = 0.0f;
    float rel = 0.0f;
    float env = 0.0f;
};

class DynamicBand
{
public:
    void prepare (float sampleRate);
    void reset();
    // Returns additive gain in dB to apply on top of the static band gain.
    float process (float sidechainSample, const BandState& band) noexcept;

private:
    float sr = 48000.0f;
    EnvelopeFollower env;
    Cascade scHp, scLp;
    float lastScLow = -1.0f;
    float lastScHigh = -1.0f;
};

} // namespace puzzleq

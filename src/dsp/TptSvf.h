#pragma once

#include <cmath>
#include <algorithm>

namespace puzzleq {

// Topology-preserving transform SVF (Zavalishin / Cytomic).
// Gain can change every sample without redesigning poles — used for dynamic EQ.
class TptSvf
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (float freqHz, float q, float sampleRate) noexcept
    {
        const float ny = sampleRate * 0.49f;
        const float f = std::clamp (freqHz, 8.0f, ny);
        g = std::tan (3.14159265f * f / sampleRate);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float tickBell (float x, float gainDb) noexcept
    {
        tick (x);
        const float A = std::pow (10.0f, gainDb * 0.025f); // /40
        return x + (A * A - 1.0f) * k * bp;
    }

    inline float tickLowShelf (float x, float gainDb) noexcept
    {
        tick (x);
        const float A2 = std::pow (10.0f, gainDb * 0.05f);
        return x + (A2 - 1.0f) * lp;
    }

    inline float tickHighShelf (float x, float gainDb) noexcept
    {
        tick (x);
        const float A2 = std::pow (10.0f, gainDb * 0.05f);
        return x + (A2 - 1.0f) * hp;
    }

    inline float tickLowpass (float x) noexcept { tick (x); return lp; }
    inline float tickHighpass (float x) noexcept { tick (x); return hp; }
    inline float tickBandpass (float x) noexcept { tick (x); return bp; }
    inline float tickNotch (float x) noexcept { tick (x); return x - k * bp; }

    float band() const noexcept { return bp; }

private:
    inline void tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }

    float ic1 = 0.0f, ic2 = 0.0f;
    float g = 0.0f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;
};

struct SmoothParam
{
    float current = 0.0f;
    float target = 0.0f;
    float coeff = 0.05f;

    void setTimeMs (float ms, float sampleRate) noexcept
    {
        const float sec = std::max (0.0005f, ms * 0.001f);
        coeff = 1.0f - std::exp (-1.0f / (sec * sampleRate));
    }

    inline float next() noexcept
    {
        current += coeff * (target - current);
        return current;
    }

    void snap() noexcept { current = target; }
};

} // namespace puzzleq

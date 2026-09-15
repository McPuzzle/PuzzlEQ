#pragma once

#include "state/Types.h"
#include <array>
#include <complex>
#include <cmath>

namespace puzzleq {

struct BiquadCoeffs
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

struct Biquad
{
    BiquadCoeffs c {};
    float z1 = 0.0f;
    float z2 = 0.0f;

    void set (const BiquadCoeffs& cc) noexcept { c = cc; }
    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

struct Cascade
{
    std::array<Biquad, kMaxSections> sections {};
    int numSections = 0;

    void clear() noexcept
    {
        numSections = 0;
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : sections)
            s.reset();
    }

    void add (const BiquadCoeffs& c)
    {
        if (numSections >= kMaxSections)
            return;
        sections[static_cast<size_t> (numSections)].set (c);
        sections[static_cast<size_t> (numSections)].reset();
        ++numSections;
    }

    inline float process (float x) noexcept
    {
        for (int i = 0; i < numSections; ++i)
            x = sections[static_cast<size_t> (i)].process (x);
        return x;
    }
};

int slopeToOrder (float slopeDbOct, bool brickwall) noexcept;

void designBand (FilterShape shape,
                 float freqHz,
                 float gainDb,
                 float q,
                 float slopeDbOct,
                 bool brickwall,
                 float sampleRate,
                 bool naturalPhase,
                 Cascade& dest);

std::complex<double> biquadResponse (const BiquadCoeffs& c, double freqHz, double sampleRate);
std::complex<double> cascadeResponse (const Cascade& cascade, double freqHz, double sampleRate);
double cascadeMagnitudeDb (const Cascade& cascade, double freqHz, double sampleRate);
double cascadePhaseRad (const Cascade& cascade, double freqHz, double sampleRate);

float noteToHz (int midiNote) noexcept;
int hzToNearestNote (float hz) noexcept;

} // namespace puzzleq

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/DynamicEngine.h"
#include "dsp/EqMatch.h"
#include "dsp/Character.h"
#include "dsp/TptSvf.h"
#include "dsp/LinearPhaseEq.h"
#include "state/BandState.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

using Catch::Matchers::WithinAbs;
using namespace puzzleq;

TEST_CASE ("Dynamic band is silent when range is zero")
{
    DynamicBand d;
    d.prepare (48000.0f);
    BandState b;
    b.dynRangeDb = 0.0f;
    b.thresholdDb = -24.0f;
    REQUIRE_THAT (d.process (0.5f, b), WithinAbs (0.0f, 1.0e-6f));
}

TEST_CASE ("Dynamic band compresses when over threshold")
{
    DynamicBand d;
    d.prepare (48000.0f);
    BandState b;
    b.dynRangeDb = -6.0f;
    b.thresholdDb = -24.0f;
    b.attackMs = 0.2f;
    b.releaseMs = 20.0f;
    float last = 0.0f;
    for (int i = 0; i < 4000; ++i)
        last = d.process (0.8f, b);
    REQUIRE (last < -1.0f);
}

TEST_CASE ("Character modes stay bounded")
{
    REQUIRE_THAT (applyCharacter (0.0f, CharacterMode::Off), WithinAbs (0.0f, 1.0e-6f));
    REQUIRE (std::abs (applyCharacter (2.0f, CharacterMode::Gentle)) <= 1.2f);
    REQUIRE (std::abs (applyCharacter (2.0f, CharacterMode::Warm)) <= 1.3f);
}

TEST_CASE ("EQ Match fits bells from a difference curve")
{
    EqMatch m;
    std::vector<float> src (1025, -20.0f);
    std::vector<float> ref (1025, -20.0f);
    // Bump reference around bin 80
    for (int i = 70; i < 95; ++i)
        ref[static_cast<size_t> (i)] = -12.0f;

    // Feed several frames
    for (int f = 0; f < 8; ++f)
        m.accumulate (src, 48000.0f, 2048);
    m.freezeAsSource();
    for (int f = 0; f < 8; ++f)
        m.accumulate (ref, 48000.0f, 2048);
    m.freezeAsReference();

    REQUIRE (m.hasSource());
    REQUIRE (m.hasReference());

    std::array<BandState, kMaxBands> bands {};
    const int n = m.fitBands (bands, 6);
    REQUIRE (n >= 1);
    bool any = false;
    for (const auto& b : bands)
        if (b.active && std::abs (b.gainDb) > 1.0f)
            any = true;
    REQUIRE (any);
}

TEST_CASE ("TPT bell peaks near the requested gain")
{
    TptSvf svf;
    svf.set (1000.0f, 1.0f, 48000.0f);
    // Drive a sine at 1 kHz long enough to settle
    const float w = 2.0f * 3.14159265f * 1000.0f / 48000.0f;
    float peak = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        const float x = std::sin (w * static_cast<float> (i));
        const float y = svf.tickBell (x, 6.0f);
        if (i > 3000)
            peak = std::max (peak, std::abs (y));
    }
    const float db = 20.0f * std::log10 (std::max (peak, 1.0e-6f));
    REQUIRE (db > 3.5f);
    REQUIRE (db < 9.0f);
}

TEST_CASE ("Linear-phase magnitude target matches the IIR curve")
{
    LinearPhaseEq lp;
    lp.prepare (48000.0f, LinearResolution::Low);
    std::array<BandState, kMaxBands> bands {};
    bands[0].active = true;
    bands[0].enabled = true;
    bands[0].shape = FilterShape::Bell;
    bands[0].frequencyHz = 1000.0f;
    bands[0].gainDb = 6.0f;
    bands[0].q = 1.0f;
    lp.updateFromBands (bands, 1.0f, -1, false);
    const float mag = lp.magnitudeAt (1000.0f);
    const float db = 20.0f * std::log10 (std::max (mag, 1.0e-8f));
    REQUIRE_THAT (db, WithinAbs (6.0f, 0.35f));
    REQUIRE_THAT (lp.magnitudeAt (40.0f), WithinAbs (1.0f, 0.08f));
}

TEST_CASE ("dbToGain is the standard mapping")
{
    REQUIRE_THAT (dbToGain (0.0f), WithinAbs (1.0f, 1.0e-5f));
    REQUIRE_THAT (dbToGain (6.0f), WithinAbs (1.99526f, 0.02f));
}

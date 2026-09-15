#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/DynamicEngine.h"
#include "dsp/EqMatch.h"
#include "dsp/Character.h"
#include "state/BandState.h"
#include <array>
#include <vector>
#include <cmath>

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

TEST_CASE ("dbToGain is the standard mapping")
{
    REQUIRE_THAT (dbToGain (0.0f), WithinAbs (1.0f, 1.0e-5f));
    REQUIRE_THAT (dbToGain (6.0f), WithinAbs (1.99526f, 0.02f));
}

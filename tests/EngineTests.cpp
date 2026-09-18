#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/DynamicEngine.h"
#include "dsp/EqMatch.h"
#include "dsp/Character.h"
#include "dsp/TptSvf.h"
#include "dsp/LinearPhaseEq.h"
#include "dsp/EqEngine.h"
#include "state/BandState.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

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

TEST_CASE ("Residual EQ Match separates two distant bumps")
{
    EqMatch m;
    std::vector<float> src (1025, -22.0f);
    std::vector<float> ref (1025, -22.0f);
    for (int i = 50; i < 70; ++i)
        ref[static_cast<size_t> (i)] = -12.0f;
    for (int i = 220; i < 250; ++i)
        ref[static_cast<size_t> (i)] = -10.0f;
    m.setSourceFrom (src, 48000.0f, 2048);
    m.setReferenceFrom (ref, 48000.0f, 2048);
    std::array<BandState, kMaxBands> bands {};
    const int n = m.fitBands (bands, 6);
    REQUIRE (n >= 2);
}

TEST_CASE ("Sketch curve fitter places a low-cut from a rising target")
{
    EqMatch m;
    std::vector<float> hz, db;
    for (int i = 0; i < 32; ++i)
    {
        const float t = static_cast<float> (i) / 31.0f;
        const float f = 20.0f * std::pow (1000.0f, t);
        hz.push_back (f);
        db.push_back (f < 120.0f ? -24.0f : 0.0f);
    }
    std::array<BandState, kMaxBands> bands {};
    const int n = m.fitTargetCurve (hz, db, bands, 6);
    REQUIRE (n >= 1);
    bool foundHp = false;
    for (const auto& b : bands)
        if (b.active && b.shape == FilterShape::LowCut)
            foundHp = true;
    REQUIRE (foundHp);
}

TEST_CASE ("EqEngine processes stereo plus sidechain at 2x without exploding")
{
    EqEngine e;
    e.prepare (48000.0f, 64);
    std::array<BandState, kMaxBands> bands {};
    bands[0].active = true;
    bands[0].enabled = true;
    bands[0].shape = FilterShape::Bell;
    bands[0].frequencyHz = 1000.0f;
    bands[0].gainDb = 3.0f;
    bands[0].q = 1.0f;
    bands[0].dynRangeDb = -4.0f;
    bands[0].trigger = DynamicTrigger::External;
    e.setBands (bands);
    GlobalState g;
    e.setGlobal (g);

    float L[64] = {}, R[64] = {}, sc[64];
    for (int i = 0; i < 64; ++i)
        sc[i] = 0.25f;
    e.process (L, R, sc, sc, 64);
    e.process (L, R, sc, sc, 64);
    float peak = 0.0f;
    for (int i = 0; i < 64; ++i)
        peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
    REQUIRE (peak < 1.0f);
    REQUIRE (std::isfinite (e.compositeMagnitudeDb (1000.0f)));
}

TEST_CASE ("zero-latency mode latency equals the 2x oversampler")
{
    Oversampler2x os;
    os.prepare (48000.0f, 64);
    EqEngine e;
    e.prepare (48000.0f, 64);
    GlobalState g;
    g.mode = ProcessingMode::ZeroLatency;
    e.setGlobal (g);
    REQUIRE (e.latencySamples() == os.latency());
}

TEST_CASE ("linear-phase mode reports additional latency")
{
    EqEngine e;
    e.prepare (48000.0f, 64);
    GlobalState g;
    g.mode = ProcessingMode::LinearPhase;
    g.lpResolution = LinearResolution::Low;
    e.setGlobal (g);
    REQUIRE (e.latencySamples() >= e.linearPhase().latencySamples());
    REQUIRE (e.latencySamples() > 0);
}

TEST_CASE ("dbToGain is the standard mapping")
{
    REQUIRE_THAT (dbToGain (0.0f), WithinAbs (1.0f, 1.0e-5f));
    REQUIRE_THAT (dbToGain (6.0f), WithinAbs (1.99526f, 0.02f));
}

TEST_CASE ("engine stays silent with no active bands")
{
    EqEngine e;
    e.prepare (48000.0f, 64);
    std::array<BandState, kMaxBands> bands {};
    e.setBands (bands);
    float L[64] = {}, R[64] = {};
    e.process (L, R, nullptr, nullptr, 64);
    float peak = 0.0f;
    for (int i = 0; i < 64; ++i)
        peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
    REQUIRE (peak < 1.0e-6f);
}

TEST_CASE ("engine chunks blocks larger than prepare size")
{
    EqEngine e;
    e.prepare (48000.0f, 64);
    std::array<BandState, kMaxBands> bands {};
    bands[0].active = true;
    bands[0].enabled = true;
    bands[0].shape = FilterShape::Bell;
    bands[0].frequencyHz = 1000.0f;
    bands[0].gainDb = 3.0f;
    bands[0].q = 1.0f;
    e.setBands (bands);

    std::vector<float> L (4096, 0.0f), R (4096, 0.0f);
    for (int i = 0; i < 4096; ++i)
        L[static_cast<size_t> (i)] = R[static_cast<size_t> (i)] = 0.1f * std::sin (0.1f * static_cast<float> (i));
    e.process (L.data(), R.data(), nullptr, nullptr, 4096);
    float peak = 0.0f;
    for (float s : L)
    {
        REQUIRE (std::isfinite (s));
        peak = std::max (peak, std::abs (s));
    }
    REQUIRE (peak < 2.0f);
}

TEST_CASE ("idle engine is cheaper than a 24-band linear-phase path")
{
    EqEngine idle, busy;
    idle.prepare (48000.0f, 128);
    busy.prepare (48000.0f, 128);
    std::array<BandState, kMaxBands> bands {};
    for (int i = 0; i < 8; ++i)
    {
        bands[static_cast<size_t> (i)].active = true;
        bands[static_cast<size_t> (i)].enabled = true;
        bands[static_cast<size_t> (i)].shape = FilterShape::Bell;
        bands[static_cast<size_t> (i)].frequencyHz = 80.0f * std::pow (1.8f, static_cast<float> (i));
        bands[static_cast<size_t> (i)].gainDb = 2.0f;
        bands[static_cast<size_t> (i)].q = 1.0f;
    }
    busy.setBands (bands);
    GlobalState g;
    g.mode = ProcessingMode::LinearPhase;
    busy.setGlobal (g);

    std::vector<float> L (128, 0.05f), R (128, 0.05f);
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 400; ++i)
        idle.process (L.data(), R.data(), nullptr, nullptr, 128);
    const auto tIdle = std::chrono::steady_clock::now() - t0;

    std::fill (L.begin(), L.end(), 0.05f);
    std::fill (R.begin(), R.end(), 0.05f);
    const auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < 400; ++i)
        busy.process (L.data(), R.data(), nullptr, nullptr, 128);
    const auto tBusy = std::chrono::steady_clock::now() - t1;
    REQUIRE (tIdle < tBusy);
}

TEST_CASE ("process stays finite across sample rates and odd block sizes")
{
    const float rates[] = { 44100.0f, 48000.0f, 96000.0f };
    const int blocks[] = { 1, 32, 63, 64, 65, 128, 511, 512, 1024, 2048, 4096 };
    for (float sr : rates)
    {
        EqEngine e;
        e.prepare (sr, 512);
        std::array<BandState, kMaxBands> bands {};
        bands[0].active = true;
        bands[0].enabled = true;
        bands[0].shape = FilterShape::Bell;
        bands[0].frequencyHz = 1000.0f;
        bands[0].gainDb = 6.0f;
        bands[0].q = 1.2f;
        bands[1].active = true;
        bands[1].enabled = true;
        bands[1].shape = FilterShape::LowCut;
        bands[1].frequencyHz = 80.0f;
        bands[1].slopeDbOct = 24.0f;
        e.setBands (bands);
        GlobalState g;
        g.autoGain = true;
        e.setGlobal (g);

        for (int n : blocks)
        {
            std::vector<float> L (static_cast<size_t> (n)), R (static_cast<size_t> (n));
            for (int i = 0; i < n; ++i)
            {
                const float x = 0.2f * std::sin (2.0f * 3.14159265f * 1000.0f * static_cast<float> (i) / sr);
                L[static_cast<size_t> (i)] = R[static_cast<size_t> (i)] = x;
            }
            e.process (L.data(), R.data(), nullptr, nullptr, n);
            for (int i = 0; i < n; ++i)
            {
                REQUIRE (std::isfinite (L[static_cast<size_t> (i)]));
                REQUIRE (std::isfinite (R[static_cast<size_t> (i)]));
                REQUIRE (std::abs (L[static_cast<size_t> (i)]) < 8.0f);
            }
        }
    }
}

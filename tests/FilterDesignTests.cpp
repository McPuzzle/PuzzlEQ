#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/FilterDesign.h"
#include "dsp/Fft.h"
#include "dsp/Oversampler2x.h"
#include "dsp/SpectrumAnalyzer.h"
#include <vector>
#include <cmath>
#include <algorithm>

using Catch::Matchers::WithinAbs;
using namespace puzzleq;

TEST_CASE ("slopeToOrder maps dB/oct and brickwall")
{
    REQUIRE (slopeToOrder (6.0f, false) == 1);
    REQUIRE (slopeToOrder (12.0f, false) == 2);
    REQUIRE (slopeToOrder (24.0f, false) == 4);
    REQUIRE (slopeToOrder (96.0f, false) == 16);
    REQUIRE (slopeToOrder (12.0f, true) == 16);
}

TEST_CASE ("Bell peaking gain at centre frequency")
{
    Cascade c;
    designBand (FilterShape::Bell, 1000.0f, 6.0f, 1.0f, 12.0f, false, 48000.0f, false, c);
    REQUIRE (c.numSections >= 1);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 1000.0, 48000.0), WithinAbs (6.0, 0.25));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 40.0, 48000.0), WithinAbs (0.0, 0.6));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 16000.0, 48000.0), WithinAbs (0.0, 0.6));
}

TEST_CASE ("Bell cut is negative at centre")
{
    Cascade c;
    designBand (FilterShape::Bell, 2000.0f, -9.0f, 1.4f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 2000.0, 48000.0), WithinAbs (-9.0, 0.3));
}

TEST_CASE ("2nd-order low cut is about -3 dB at fc")
{
    Cascade c;
    designBand (FilterShape::LowCut, 1000.0f, 0.0f, 0.707f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 1000.0, 48000.0), WithinAbs (-3.0, 0.6));
    REQUIRE (cascadeMagnitudeDb (c, 100.0, 48000.0) < -15.0);
    REQUIRE (cascadeMagnitudeDb (c, 8000.0, 48000.0) > -1.0);
}

TEST_CASE ("2nd-order high cut is about -3 dB at fc")
{
    Cascade c;
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 2000.0, 48000.0), WithinAbs (-3.0, 0.6));
    REQUIRE (cascadeMagnitudeDb (c, 200.0, 48000.0) > -1.0);
}

TEST_CASE ("High shelf reaches target in the highs")
{
    Cascade c;
    designBand (FilterShape::HighShelf, 2000.0f, 6.0f, 0.707f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 16000.0, 48000.0), WithinAbs (6.0, 0.8));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 40.0, 48000.0), WithinAbs (0.0, 0.8));
}

TEST_CASE ("Low shelf reaches target in the lows")
{
    Cascade c;
    designBand (FilterShape::LowShelf, 200.0f, 4.0f, 0.707f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 40.0, 48000.0), WithinAbs (4.0, 0.8));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 12000.0, 48000.0), WithinAbs (0.0, 0.8));
}

TEST_CASE ("Notch is deep at centre")
{
    Cascade c;
    designBand (FilterShape::Notch, 1000.0f, 0.0f, 4.0f, 12.0f, false, 48000.0f, false, c);
    REQUIRE (cascadeMagnitudeDb (c, 1000.0, 48000.0) < -20.0);
}

TEST_CASE ("All-pass is unity magnitude")
{
    Cascade c;
    designBand (FilterShape::AllPass, 800.0f, 0.0f, 0.707f, 12.0f, false, 48000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 100.0, 48000.0), WithinAbs (0.0, 0.15));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 800.0, 48000.0), WithinAbs (0.0, 0.15));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 8000.0, 48000.0), WithinAbs (0.0, 0.15));
}

TEST_CASE ("IIR cascade is zero-latency and silent on silence")
{
    Cascade c;
    designBand (FilterShape::Bell, 1000.0f, 6.0f, 1.0f, 12.0f, false, 48000.0f, false, c);
    float maxAbs = 0.0f;
    for (int i = 0; i < 2048; ++i)
        maxAbs = std::max (maxAbs, std::abs (c.process (0.0f)));
    REQUIRE (maxAbs < 1.0e-6f);
}

TEST_CASE ("Natural phase keeps the same magnitude as zero latency")
{
    Cascade zl, nat;
    designBand (FilterShape::Bell, 1500.0f, 5.0f, 1.0f, 12.0f, false, 48000.0f, false, zl);
    designBand (FilterShape::Bell, 1500.0f, 5.0f, 1.0f, 12.0f, false, 48000.0f, true, nat);
    REQUIRE_THAT (cascadeMagnitudeDb (nat, 1500.0, 48000.0),
                  WithinAbs (cascadeMagnitudeDb (zl, 1500.0, 48000.0), 0.35));
}

TEST_CASE ("note / hz conversion is stable")
{
    REQUIRE_THAT (noteToHz (69), WithinAbs (440.0f, 0.01));
    REQUIRE (hzToNearestNote (440.0f) == 69);
}

TEST_CASE ("FFT impulse is flat-ish magnitude")
{
    Fft fft;
    fft.setup (256);
    std::vector<float> re (256, 0.0f), im (256, 0.0f);
    re[0] = 1.0f;
    fft.forward (re.data(), im.data());
    for (int i = 0; i < 256; ++i)
    {
        const float mag = std::sqrt (re[static_cast<size_t> (i)] * re[static_cast<size_t> (i)]
                                     + im[static_cast<size_t> (i)] * im[static_cast<size_t> (i)]);
        REQUIRE_THAT (mag, WithinAbs (1.0f, 1.0e-4f));
    }
}

TEST_CASE ("Fractional 15 dB/oct high-cut sits between 12 and 24")
{
    Cascade c12, c15, c24;
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 12.0f, false, 48000.0f, false, c12);
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 15.0f, false, 48000.0f, false, c15);
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 24.0f, false, 48000.0f, false, c24);
    const double m12 = cascadeMagnitudeDb (c12, 4000.0, 48000.0);
    const double m15 = cascadeMagnitudeDb (c15, 4000.0, 48000.0);
    const double m24 = cascadeMagnitudeDb (c24, 4000.0, 48000.0);
    REQUIRE (m15 < m12);
    REQUIRE (m15 > m24);
}

TEST_CASE ("12 kHz analog-matched bell still peaks at +6 dB at 2x rate")
{
    Cascade c;
    designBand (FilterShape::Bell, 12000.0f, 6.0f, 1.0f, 12.0f, false, 96000.0f, false, c);
    REQUIRE_THAT (cascadeMagnitudeDb (c, 12000.0, 96000.0), WithinAbs (6.0, 0.25));
    REQUIRE_THAT (cascadeMagnitudeDb (c, 200.0, 96000.0), WithinAbs (0.0, 0.6));
}

TEST_CASE ("Oversampler 2x is silent on silence and reports latency")
{
    Oversampler2x os;
    os.prepare (48000.0f, 64);
    REQUIRE (os.latency() > 0);
    float l[64] = {}, r[64] = {};
    os.upsample (l, r, 64);
    os.downsample (l, r, 64);
    float peak = 0.0f;
    for (int i = 0; i < 64; ++i)
        peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));
    REQUIRE (peak < 1.0e-5f);
}

TEST_CASE ("Parabolic peak interpolation is exact on a quadratic")
{
    // y = -(x-0.3)^2 so peak at +0.3 bins
    const float ym1 = -(-1.0f - 0.3f) * (-1.0f - 0.3f);
    const float y0  = -(0.0f - 0.3f) * (0.0f - 0.3f);
    const float yp1 = -(1.0f - 0.3f) * (1.0f - 0.3f);
    REQUIRE_THAT (SpectrumAnalyzer::parabolicDelta (ym1, y0, yp1), WithinAbs (0.3f, 0.02f));
}

TEST_CASE ("tiny inputs stay finite (no denormal blow-up)")
{
    Cascade c;
    designBand (FilterShape::Bell, 1000.0f, 6.0f, 1.0f, 12.0f, false, 48000.0f, false, c);
    (void) c.process (1.0e-20f);
    for (int i = 0; i < 8192; ++i)
    {
        const float y = c.process (0.0f);
        REQUIRE (std::isfinite (y));
        if (i > 4096)
            REQUIRE (std::abs (y) < 1.0e-6f);
    }
}

TEST_CASE ("brickwall high-cut is steeper than 96 dB/oct at 12 dB labelled slope")
{
    Cascade regular, brick;
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 12.0f, false, 48000.0f, false, regular);
    designBand (FilterShape::HighCut, 2000.0f, 0.0f, 0.707f, 12.0f, true, 48000.0f, false, brick);
    REQUIRE (cascadeMagnitudeDb (brick, 8000.0, 48000.0)
             < cascadeMagnitudeDb (regular, 8000.0, 48000.0) - 10.0);
}

TEST_CASE ("FFT inverse restores a sine")
{
    Fft fft;
    fft.setup (128);
    std::vector<float> re (128), im (128, 0.0f);
    for (int i = 0; i < 128; ++i)
        re[static_cast<size_t> (i)] = std::sin (2.0f * 3.14159265f * 4.0f * static_cast<float> (i) / 128.0f);
    auto original = re;
    fft.forward (re.data(), im.data());
    fft.inverse (re.data(), im.data());
    for (int i = 0; i < 128; ++i)
        REQUIRE_THAT (re[static_cast<size_t> (i)], WithinAbs (original[static_cast<size_t> (i)], 1.0e-4f));
}

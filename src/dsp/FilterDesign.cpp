#include "dsp/FilterDesign.h"
#include <algorithm>

namespace puzzleq {

namespace {

constexpr double kPi = 3.14159265358979323846;

BiquadCoeffs identity()
{
    return { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}

BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    if (std::abs (a0) < 1.0e-12)
        return identity();
    BiquadCoeffs c;
    c.b0 = static_cast<float> (b0 / a0);
    c.b1 = static_cast<float> (b1 / a0);
    c.b2 = static_cast<float> (b2 / a0);
    c.a1 = static_cast<float> (a1 / a0);
    c.a2 = static_cast<float> (a2 / a0);
    return c;
}

double clampFreq (double hz, double fs)
{
    const double ny = fs * 0.49;
    return std::min (std::max (hz, 2.0), ny);
}

BiquadCoeffs firstOrderLowpass (double hz, double fs)
{
    const double f = clampFreq (hz, fs);
    const double k = std::tan (kPi * f / fs);
    return normalise (k, k, 0.0, 1.0 + k, k - 1.0, 0.0);
}

BiquadCoeffs firstOrderHighpass (double hz, double fs)
{
    const double f = clampFreq (hz, fs);
    const double k = std::tan (kPi * f / fs);
    return normalise (1.0, -1.0, 0.0, 1.0 + k, k - 1.0, 0.0);
}

BiquadCoeffs firstOrderAllpass (double hz, double fs)
{
    const double f = clampFreq (hz, fs);
    const double k = std::tan (kPi * f / fs);
    const double a = (k - 1.0) / (k + 1.0);
    return normalise (a, 1.0, 0.0, 1.0, a, 0.0);
}

BiquadCoeffs firstOrderLowShelf (double hz, double gainDb, double fs)
{
    const double f = clampFreq (hz, fs);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double k = std::tan (kPi * f / fs);
    // Orfanidis / first-order shelf
    const double b0 = A * k + A * A;
    const double b1 = A * k - A * A;
    const double a0 = k + A;
    const double a1 = k - A;
    return normalise (b0, b1, 0.0, a0, a1, 0.0);
}

[[maybe_unused]] BiquadCoeffs firstOrderHighShelf (double hz, double gainDb, double fs)
{
    const double f = clampFreq (hz, fs);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double k = std::tan (kPi * f / fs);
    const double b0 = A * A * k + A;
    const double b1 = A * A * k - A;
    const double a0 = A * k + 1.0;
    const double a1 = A * k - 1.0;
    return normalise (b0, b1, 0.0, a0, a1, 0.0);
}

struct Analog
{
    double w0, cosw, sinw, alpha, A;
};

Analog rbj (double hz, double q, double gainDb, double fs)
{
    Analog a;
    const double f = clampFreq (hz, fs);
    a.w0 = 2.0 * kPi * f / fs;
    a.cosw = std::cos (a.w0);
    a.sinw = std::sin (a.w0);
    a.alpha = a.sinw / (2.0 * std::max (q, 0.05));
    a.A = std::pow (10.0, gainDb / 40.0);
    return a;
}

BiquadCoeffs peaking (double hz, double gainDb, double q, double fs)
{
    const auto a = rbj (hz, q, gainDb, fs);
    return normalise (1.0 + a.alpha * a.A,
                      -2.0 * a.cosw,
                      1.0 - a.alpha * a.A,
                      1.0 + a.alpha / a.A,
                      -2.0 * a.cosw,
                      1.0 - a.alpha / a.A);
}

BiquadCoeffs notch (double hz, double q, double fs)
{
    const auto a = rbj (hz, q, 0.0, fs);
    return normalise (1.0, -2.0 * a.cosw, 1.0,
                      1.0 + a.alpha, -2.0 * a.cosw, 1.0 - a.alpha);
}

BiquadCoeffs bandpass (double hz, double q, double fs)
{
    const auto a = rbj (hz, q, 0.0, fs);
    return normalise (a.alpha, 0.0, -a.alpha,
                      1.0 + a.alpha, -2.0 * a.cosw, 1.0 - a.alpha);
}

BiquadCoeffs lowShelf2 (double hz, double gainDb, double q, double fs)
{
    const auto a = rbj (hz, q, gainDb, fs);
    const double twoSqrtAalpha = 2.0 * std::sqrt (a.A) * a.alpha;
    return normalise (a.A * ((a.A + 1.0) - (a.A - 1.0) * a.cosw + twoSqrtAalpha),
                      2.0 * a.A * ((a.A - 1.0) - (a.A + 1.0) * a.cosw),
                      a.A * ((a.A + 1.0) - (a.A - 1.0) * a.cosw - twoSqrtAalpha),
                      (a.A + 1.0) + (a.A - 1.0) * a.cosw + twoSqrtAalpha,
                      -2.0 * ((a.A - 1.0) + (a.A + 1.0) * a.cosw),
                      (a.A + 1.0) + (a.A - 1.0) * a.cosw - twoSqrtAalpha);
}

BiquadCoeffs highShelf2 (double hz, double gainDb, double q, double fs)
{
    const auto a = rbj (hz, q, gainDb, fs);
    const double twoSqrtAalpha = 2.0 * std::sqrt (a.A) * a.alpha;
    return normalise (a.A * ((a.A + 1.0) + (a.A - 1.0) * a.cosw + twoSqrtAalpha),
                      -2.0 * a.A * ((a.A - 1.0) + (a.A + 1.0) * a.cosw),
                      a.A * ((a.A + 1.0) + (a.A - 1.0) * a.cosw - twoSqrtAalpha),
                      (a.A + 1.0) - (a.A - 1.0) * a.cosw + twoSqrtAalpha,
                      2.0 * ((a.A - 1.0) - (a.A + 1.0) * a.cosw),
                      (a.A + 1.0) - (a.A - 1.0) * a.cosw - twoSqrtAalpha);
}

BiquadCoeffs allpass2 (double hz, double q, double fs)
{
    const auto a = rbj (hz, q, 0.0, fs);
    return normalise (1.0 - a.alpha, -2.0 * a.cosw, 1.0 + a.alpha,
                      1.0 + a.alpha, -2.0 * a.cosw, 1.0 - a.alpha);
}

BiquadCoeffs butterworthLp2 (double hz, double q, double fs)
{
    const auto a = rbj (hz, q, 0.0, fs);
    return normalise ((1.0 - a.cosw) * 0.5, 1.0 - a.cosw, (1.0 - a.cosw) * 0.5,
                      1.0 + a.alpha, -2.0 * a.cosw, 1.0 - a.alpha);
}

BiquadCoeffs butterworthHp2 (double hz, double q, double fs)
{
    const auto a = rbj (hz, q, 0.0, fs);
    return normalise ((1.0 + a.cosw) * 0.5, -(1.0 + a.cosw), (1.0 + a.cosw) * 0.5,
                      1.0 + a.alpha, -2.0 * a.cosw, 1.0 - a.alpha);
}

void addButterworthLp (Cascade& dest, double hz, int order, double fs)
{
    if (order <= 0)
        return;
    if (order % 2 == 1)
    {
        dest.add (firstOrderLowpass (hz, fs));
        --order;
    }
    const int pairs = order / 2;
    for (int k = 0; k < pairs; ++k)
    {
        const double q = 1.0 / (2.0 * std::sin ((2.0 * k + 1.0) * kPi / (2.0 * (pairs * 2))));
        dest.add (butterworthLp2 (hz, q, fs));
    }
}

void addButterworthHp (Cascade& dest, double hz, int order, double fs)
{
    if (order <= 0)
        return;
    if (order % 2 == 1)
    {
        dest.add (firstOrderHighpass (hz, fs));
        --order;
    }
    const int pairs = order / 2;
    for (int k = 0; k < pairs; ++k)
    {
        const double q = 1.0 / (2.0 * std::sin ((2.0 * k + 1.0) * kPi / (2.0 * (pairs * 2))));
        dest.add (butterworthHp2 (hz, q, fs));
    }
}

} // namespace

int slopeToOrder (float slopeDbOct, bool brickwall) noexcept
{
    if (brickwall)
        return 16;
    const int order = static_cast<int> (std::lround (slopeDbOct / 6.0f));
    return std::clamp (order, 1, 16);
}

void designBand (FilterShape shape,
                 float freqHz,
                 float gainDb,
                 float q,
                 float slopeDbOct,
                 bool brickwall,
                 float sampleRate,
                 bool naturalPhase,
                 Cascade& dest)
{
    dest.clear();
    const double fs = static_cast<double> (sampleRate);
    const double f = static_cast<double> (freqHz);
    const double g = static_cast<double> (gainDb);
    const double qq = std::max (0.05, static_cast<double> (q));
    const int order = slopeToOrder (slopeDbOct, brickwall);

    switch (shape)
    {
        case FilterShape::Bell:
            dest.add (peaking (f, g, qq, fs));
            break;

        case FilterShape::Notch:
            dest.add (notch (f, qq, fs));
            if (order >= 4)
                dest.add (notch (f, qq * 0.85, fs));
            break;

        case FilterShape::HighShelf:
            if (order <= 2)
                dest.add (highShelf2 (f, g, 0.707, fs));
            else
            {
                // High-order shelf: dry + (gain-1)*HP
                // Implemented as cascaded 2nd-order shelves splitting the gain.
                const int stages = std::max (1, order / 2);
                const double gs = g / static_cast<double> (stages);
                for (int i = 0; i < stages; ++i)
                    dest.add (highShelf2 (f, gs, 0.707, fs));
            }
            break;

        case FilterShape::LowShelf:
            if (order <= 2)
                dest.add (lowShelf2 (f, g, 0.707, fs));
            else
            {
                const int stages = std::max (1, order / 2);
                const double gs = g / static_cast<double> (stages);
                for (int i = 0; i < stages; ++i)
                    dest.add (lowShelf2 (f, gs, 0.707, fs));
            }
            break;

        case FilterShape::HighCut:
            addButterworthLp (dest, f, order, fs);
            break;

        case FilterShape::LowCut:
            addButterworthHp (dest, f, order, fs);
            break;

        case FilterShape::BandPass:
        {
            dest.add (bandpass (f, qq, fs));
            const int extra = std::max (0, order / 2 - 1);
            for (int i = 0; i < extra; ++i)
                dest.add (bandpass (f, qq, fs));
            break;
        }

        case FilterShape::TiltShelf:
            dest.add (lowShelf2 (f, g * 0.5, 0.707, fs));
            dest.add (highShelf2 (f, -g * 0.5, 0.707, fs));
            if (order >= 4)
            {
                dest.add (lowShelf2 (f, g * 0.15, 0.5, fs));
                dest.add (highShelf2 (f, -g * 0.15, 0.5, fs));
            }
            break;

        case FilterShape::FlatTilt:
        {
            constexpr int n = 8;
            const double gs = g / static_cast<double> (n);
            for (int i = 0; i < n; ++i)
            {
                const double t = (i + 0.5) / static_cast<double> (n);
                const double hf = 20.0 * std::pow (1000.0, t);
                dest.add (firstOrderLowShelf (hf, gs, fs));
            }
            break;
        }

        case FilterShape::AllPass:
            if (order <= 1)
                dest.add (firstOrderAllpass (f, fs));
            else
                dest.add (allpass2 (f, qq, fs));
            break;

        case FilterShape::NumShapes:
            break;
    }

    if (naturalPhase && dest.numSections > 0
        && shape != FilterShape::AllPass && shape != FilterShape::FlatTilt)
    {
        dest.add (firstOrderAllpass (f, fs));
    }
}

std::complex<double> biquadResponse (const BiquadCoeffs& c, double freqHz, double sampleRate)
{
    const double w = 2.0 * kPi * freqHz / sampleRate;
    const std::complex<double> z1 { std::cos (w), -std::sin (w) };
    const std::complex<double> z2 = z1 * z1;
    const std::complex<double> num = static_cast<double> (c.b0)
                                   + static_cast<double> (c.b1) * z1
                                   + static_cast<double> (c.b2) * z2;
    const std::complex<double> den = 1.0
                                   + static_cast<double> (c.a1) * z1
                                   + static_cast<double> (c.a2) * z2;
    if (std::abs (den) < 1.0e-20)
        return { 1.0, 0.0 };
    return num / den;
}

std::complex<double> cascadeResponse (const Cascade& cascade, double freqHz, double sampleRate)
{
    std::complex<double> h { 1.0, 0.0 };
    for (int i = 0; i < cascade.numSections; ++i)
        h *= biquadResponse (cascade.sections[static_cast<size_t> (i)].c, freqHz, sampleRate);
    return h;
}

double cascadeMagnitudeDb (const Cascade& cascade, double freqHz, double sampleRate)
{
    const double mag = std::abs (cascadeResponse (cascade, freqHz, sampleRate));
    if (mag < 1.0e-12)
        return -240.0;
    return 20.0 * std::log10 (mag);
}

double cascadePhaseRad (const Cascade& cascade, double freqHz, double sampleRate)
{
    return std::arg (cascadeResponse (cascade, freqHz, sampleRate));
}

float noteToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (midiNote) - 69.0f) / 12.0f);
}

int hzToNearestNote (float hz) noexcept
{
    if (hz <= 0.0f)
        return 0;
    return static_cast<int> (std::lround (69.0f + 12.0f * std::log2 (hz / 440.0f)));
}

} // namespace puzzleq

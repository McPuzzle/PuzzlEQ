#include "dsp/EqMatch.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/FilterDesign.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

namespace {

int firstFreeSlot (const std::array<BandState, kMaxBands>& dest)
{
    for (int i = 0; i < kMaxBands; ++i)
        if (! dest[static_cast<size_t> (i)].active)
            return i;
    return -1;
}

float estimateQ (const std::vector<float>& sm, int peak, float sampleRate, int fftSize)
{
    const int bins = static_cast<int> (sm.size());
    const float peakV = sm[static_cast<size_t> (peak)];
    const float half = peakV * 0.5f;
    int L = peak, R = peak;
    while (L > 2 && std::abs (sm[static_cast<size_t> (L)]) > std::abs (half))
        --L;
    while (R < bins - 2 && std::abs (sm[static_cast<size_t> (R)]) > std::abs (half))
        ++R;
    const float f0 = SpectrumAnalyzer::binToHz (peak, fftSize, sampleRate);
    const float f1 = SpectrumAnalyzer::binToHz (std::max (1, L), fftSize, sampleRate);
    const float f2 = SpectrumAnalyzer::binToHz (R, fftSize, sampleRate);
    const float bw = std::max (f2 - f1, f0 * 0.08f);
    return std::clamp (f0 / bw, 0.3f, 8.0f);
}

void subtractBand (std::vector<float>& residual, const BandState& b, float sampleRate, int fftSize)
{
    Cascade c;
    designBand (b.shape, b.frequencyHz, b.gainDb, b.q, b.slopeDbOct, false, sampleRate, false, c);
    const int bins = static_cast<int> (residual.size());
    for (int i = 1; i < bins; ++i)
    {
        const float hz = SpectrumAnalyzer::binToHz (i, fftSize, sampleRate);
        residual[static_cast<size_t> (i)] -= static_cast<float> (cascadeMagnitudeDb (c, hz, sampleRate));
    }
}

float meanRange (const std::vector<float>& v, int a, int b)
{
    a = std::clamp (a, 1, static_cast<int> (v.size()) - 1);
    b = std::clamp (b, a, static_cast<int> (v.size()) - 1);
    float s = 0.0f;
    int n = 0;
    for (int i = a; i <= b; ++i)
    {
        s += v[static_cast<size_t> (i)];
        ++n;
    }
    return n > 0 ? s / static_cast<float> (n) : 0.0f;
}

} // namespace

void EqMatch::reset()
{
    accum.clear();
    sourceDb.clear();
    refDb.clear();
    frames = 0;
    sourceReady = refReady = false;
}

void EqMatch::accumulate (const std::vector<float>& magDb, float sampleRate, int fftSize)
{
    sr = sampleRate;
    fftN = fftSize;
    if (accum.size() != magDb.size())
    {
        accum.assign (magDb.size(), 0.0f);
        frames = 0;
    }
    for (size_t i = 0; i < magDb.size(); ++i)
        accum[i] += magDb[i];
    ++frames;
}

void EqMatch::setSourceFrom (const std::vector<float>& magDb, float sampleRate, int fftSize)
{
    sourceDb = magDb;
    sr = sampleRate;
    fftN = fftSize;
    sourceReady = sourceDb.size() >= 8;
}

void EqMatch::setReferenceFrom (const std::vector<float>& magDb, float sampleRate, int fftSize)
{
    refDb = magDb;
    sr = sampleRate;
    fftN = fftSize;
    refReady = refDb.size() >= 8;
}

void EqMatch::freezeAsSource()
{
    if (frames <= 0)
        return;
    sourceDb.resize (accum.size());
    const float inv = 1.0f / static_cast<float> (frames);
    for (size_t i = 0; i < accum.size(); ++i)
        sourceDb[i] = accum[i] * inv;
    sourceReady = true;
    accum.assign (accum.size(), 0.0f);
    frames = 0;
}

void EqMatch::freezeAsReference()
{
    if (frames <= 0)
        return;
    refDb.resize (accum.size());
    const float inv = 1.0f / static_cast<float> (frames);
    for (size_t i = 0; i < accum.size(); ++i)
        refDb[i] = accum[i] * inv;
    refReady = true;
    accum.assign (accum.size(), 0.0f);
    frames = 0;
}

int EqMatch::fitResidual (std::vector<float> residual,
                          float sampleRate,
                          int fftSize,
                          std::array<BandState, kMaxBands>& dest,
                          int maxBands) const
{
    const int bins = static_cast<int> (residual.size());
    if (bins < 8 || maxBands <= 0)
        return 0;

    std::vector<float> usedHz;
    int written = 0;

    while (written < maxBands)
    {
        int best = -1;
        float bestAbs = 1.15f;
        for (int i = 4; i < bins - 4; ++i)
        {
            const float hz = SpectrumAnalyzer::binToHz (i, fftSize, sampleRate);
            if (hz < 30.0f || hz > 16000.0f)
                continue;
            bool near = false;
            for (float u : usedHz)
            {
                if (std::abs (std::log2 (std::max (hz, 1.0f) / std::max (u, 1.0f))) < 0.32f)
                {
                    near = true;
                    break;
                }
            }
            if (near)
                continue;

            const float v = residual[static_cast<size_t> (i)];
            if (std::abs (v) < bestAbs)
                continue;
            if (std::abs (v) >= std::abs (residual[static_cast<size_t> (i - 1)])
                && std::abs (v) >= std::abs (residual[static_cast<size_t> (i + 1)]))
            {
                bestAbs = std::abs (v);
                best = i;
            }
        }
        if (best < 0)
            break;

        const int slot = firstFreeSlot (dest);
        if (slot < 0)
            break;

        const float delta = SpectrumAnalyzer::parabolicDelta (
            residual[static_cast<size_t> (best - 1)],
            residual[static_cast<size_t> (best)],
            residual[static_cast<size_t> (best + 1)]);
        const float hz = std::clamp ((static_cast<float> (best) + delta) * sampleRate
                                         / static_cast<float> (fftSize),
                                     kMinHz, kMaxHz);

        BandState b;
        b.active = true;
        b.enabled = true;
        b.shape = FilterShape::Bell;
        b.frequencyHz = hz;
        b.gainDb = std::clamp (residual[static_cast<size_t> (best)], kMinGainDb, kMaxGainDb);
        b.q = estimateQ (residual, best, sampleRate, fftSize);
        dest[static_cast<size_t> (slot)] = b;
        usedHz.push_back (b.frequencyHz);
        subtractBand (residual, b, sampleRate, fftSize);
        ++written;
    }
    return written;
}

int EqMatch::fitBands (std::array<BandState, kMaxBands>& dest, int maxBands) const
{
    if (! sourceReady || ! refReady || sourceDb.size() != refDb.size() || sourceDb.size() < 8)
        return 0;

    const int bins = static_cast<int> (sourceDb.size());
    std::vector<float> diff (static_cast<size_t> (bins), 0.0f);
    for (int i = 1; i < bins; ++i)
        diff[static_cast<size_t> (i)] = refDb[static_cast<size_t> (i)] - sourceDb[static_cast<size_t> (i)];

    std::vector<float> sm = diff;
    for (int i = 2; i < bins - 2; ++i)
        sm[static_cast<size_t> (i)] = 0.1f * diff[static_cast<size_t> (i - 2)]
                                    + 0.2f * diff[static_cast<size_t> (i - 1)]
                                    + 0.4f * diff[static_cast<size_t> (i)]
                                    + 0.2f * diff[static_cast<size_t> (i + 1)]
                                    + 0.1f * diff[static_cast<size_t> (i + 2)];

    return fitResidual (std::move (sm), sr, fftN, dest, maxBands);
}

int EqMatch::fitTargetCurve (const std::vector<float>& freqsHz,
                             const std::vector<float>& targetDb,
                             std::array<BandState, kMaxBands>& dest,
                             int maxBands) const
{
    if (freqsHz.size() < 4 || freqsHz.size() != targetDb.size() || maxBands <= 0)
        return 0;

    const float sampleRate = 48000.0f;
    const int fftSize = 2048;
    const int bins = fftSize / 2 + 1;
    std::vector<float> curve (static_cast<size_t> (bins), 0.0f);

    std::vector<int> order (freqsHz.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<int> (i);
    std::sort (order.begin(), order.end(), [&] (int a, int b)
    {
        return freqsHz[static_cast<size_t> (a)] < freqsHz[static_cast<size_t> (b)];
    });

    auto sampleAt = [&] (float hz) -> float
    {
        if (hz <= freqsHz[static_cast<size_t> (order.front())])
            return targetDb[static_cast<size_t> (order.front())];
        if (hz >= freqsHz[static_cast<size_t> (order.back())])
            return targetDb[static_cast<size_t> (order.back())];
        for (size_t i = 1; i < order.size(); ++i)
        {
            const float h0 = freqsHz[static_cast<size_t> (order[i - 1])];
            const float h1 = freqsHz[static_cast<size_t> (order[i])];
            if (hz <= h1)
            {
                const float t = (std::log (hz) - std::log (h0)) / std::max (1.0e-6f, std::log (h1) - std::log (h0));
                const float d0 = targetDb[static_cast<size_t> (order[i - 1])];
                const float d1 = targetDb[static_cast<size_t> (order[i])];
                return d0 + t * (d1 - d0);
            }
        }
        return 0.0f;
    };

    for (int i = 1; i < bins; ++i)
        curve[static_cast<size_t> (i)] = sampleAt (SpectrumAnalyzer::binToHz (i, fftSize, sampleRate));

    int written = 0;
    const int lowA = SpectrumAnalyzer::hzToBin (25.0f, fftSize, sampleRate);
    const int lowB = SpectrumAnalyzer::hzToBin (120.0f, fftSize, sampleRate);
    const int midA = SpectrumAnalyzer::hzToBin (300.0f, fftSize, sampleRate);
    const int midB = SpectrumAnalyzer::hzToBin (2500.0f, fftSize, sampleRate);
    const int hiA = SpectrumAnalyzer::hzToBin (6000.0f, fftSize, sampleRate);
    const int hiB = SpectrumAnalyzer::hzToBin (16000.0f, fftSize, sampleRate);
    const float lowM = meanRange (curve, lowA, lowB);
    const float midM = meanRange (curve, midA, midB);
    const float hiM = meanRange (curve, hiA, hiB);

    auto place = [&] (FilterShape shape, float hz, float gain, float q, float slope) -> bool
    {
        if (written >= maxBands)
            return false;
        const int slot = firstFreeSlot (dest);
        if (slot < 0)
            return false;
        BandState b;
        b.active = true;
        b.enabled = true;
        b.shape = shape;
        b.frequencyHz = std::clamp (hz, kMinHz, kMaxHz);
        b.gainDb = std::clamp (gain, kMinGainDb, kMaxGainDb);
        b.q = std::clamp (q, kMinQ, kMaxQ);
        b.slopeDbOct = std::clamp (slope, kMinSlope, kMaxSlope);
        dest[static_cast<size_t> (slot)] = b;
        subtractBand (curve, b, sampleRate, fftSize);
        ++written;
        return true;
    };

    if (lowM < midM - 7.0f)
    {
        float cross = 80.0f;
        for (int i = lowA; i < midA; ++i)
        {
            if (curve[static_cast<size_t> (i)] > midM - 3.0f)
            {
                cross = SpectrumAnalyzer::binToHz (i, fftSize, sampleRate);
                break;
            }
        }
        const float rise = meanRange (curve, midA, midA + 8) - lowM;
        place (FilterShape::LowCut, cross, 0.0f, 0.707f, rise > 18.0f ? 24.0f : 12.0f);
    }
    else if (std::abs (lowM - midM) > 1.7f)
    {
        place (FilterShape::LowShelf, 180.0f, lowM - midM, 0.707f, 12.0f);
    }

    if (hiM < midM - 7.0f)
    {
        float cross = 8000.0f;
        for (int i = hiB; i > midB; --i)
        {
            if (curve[static_cast<size_t> (i)] > midM - 3.0f)
            {
                cross = SpectrumAnalyzer::binToHz (i, fftSize, sampleRate);
                break;
            }
        }
        const float drop = midM - hiM;
        place (FilterShape::HighCut, cross, 0.0f, 0.707f, drop > 18.0f ? 24.0f : 12.0f);
    }
    else if (std::abs (hiM - midM) > 1.7f)
    {
        place (FilterShape::HighShelf, 4500.0f, hiM - midM, 0.707f, 12.0f);
    }

    written += fitResidual (std::move (curve), sampleRate, fftSize, dest, maxBands - written);
    return written;
}

} // namespace puzzleq

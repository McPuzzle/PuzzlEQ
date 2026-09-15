#include "dsp/EqMatch.h"
#include "dsp/SpectrumAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

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

int EqMatch::fitBands (std::array<BandState, kMaxBands>& dest, int maxBands) const
{
    if (! sourceReady || ! refReady || sourceDb.size() != refDb.size() || sourceDb.size() < 8)
        return 0;

    const int bins = static_cast<int> (sourceDb.size());
    std::vector<float> diff (static_cast<size_t> (bins), 0.0f);
    for (int i = 1; i < bins; ++i)
        diff[static_cast<size_t> (i)] = refDb[static_cast<size_t> (i)] - sourceDb[static_cast<size_t> (i)];

    // Smooth
    std::vector<float> sm = diff;
    for (int i = 2; i < bins - 2; ++i)
        sm[static_cast<size_t> (i)] = 0.1f * diff[static_cast<size_t> (i - 2)]
                                    + 0.2f * diff[static_cast<size_t> (i - 1)]
                                    + 0.4f * diff[static_cast<size_t> (i)]
                                    + 0.2f * diff[static_cast<size_t> (i + 1)]
                                    + 0.1f * diff[static_cast<size_t> (i + 2)];

    struct Peak { int bin; float mag; };
    std::vector<Peak> peaks;
    for (int i = 4; i < bins - 4; ++i)
    {
        const float hz = SpectrumAnalyzer::binToHz (i, (bins - 1) * 2, sr);
        if (hz < 30.0f || hz > 16000.0f)
            continue;
        const float v = sm[static_cast<size_t> (i)];
        if (std::abs (v) < 1.2f)
            continue;
        if (std::abs (v) >= std::abs (sm[static_cast<size_t> (i - 1)])
            && std::abs (v) >= std::abs (sm[static_cast<size_t> (i + 1)]))
            peaks.push_back ({ i, v });
    }

    std::sort (peaks.begin(), peaks.end(), [] (const Peak& a, const Peak& b)
    {
        return std::abs (a.mag) > std::abs (b.mag);
    });

    int written = 0;
    for (const auto& p : peaks)
    {
        if (written >= maxBands)
            break;

        // Find a free slot
        int slot = -1;
        for (int i = 0; i < kMaxBands; ++i)
        {
            if (! dest[static_cast<size_t> (i)].active)
            {
                slot = i;
                break;
            }
        }
        if (slot < 0)
            break;

        BandState b;
        b.active = true;
        b.enabled = true;
        b.shape = FilterShape::Bell;
        b.frequencyHz = SpectrumAnalyzer::binToHz (p.bin, (bins - 1) * 2, sr);
        b.gainDb = std::clamp (p.mag, kMinGainDb, kMaxGainDb);
        b.q = 1.1f;
        dest[static_cast<size_t> (slot)] = b;
        ++written;
    }
    return written;
}

} // namespace puzzleq

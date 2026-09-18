#include "assistant/EqAnalyze.h"
#include "dsp/SpectrumAnalyzer.h"
#include "state/Types.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace puzzleq {
namespace {

float binHz (int bin, int fftSize, float sr)
{
    return SpectrumAnalyzer::binToHz (bin, fftSize, sr);
}

int hzBin (float hz, int fftSize, float sr)
{
    return SpectrumAnalyzer::hzToBin (hz, fftSize, sr);
}

float localMedian (const std::vector<float>& v, int center, int radius)
{
    const int n = static_cast<int> (v.size());
    const int lo = std::max (0, center - radius);
    const int hi = std::min (n - 1, center + radius);
    std::vector<float> w;
    w.reserve (static_cast<size_t> (hi - lo + 1));
    for (int i = lo; i <= hi; ++i)
        w.push_back (v[static_cast<size_t> (i)]);
    const auto mid = w.begin() + static_cast<std::ptrdiff_t> (w.size() / 2);
    std::nth_element (w.begin(), mid, w.end());
    return *mid;
}

float vocalHarshWeight (float hz)
{
    // Extra sensitivity in the classic vocal ice-pick / hardness pocket.
    if (hz >= 2800.0f && hz <= 5200.0f)
        return 2.4f;
    if (hz >= 2200.0f && hz <= 8000.0f)
        return 1.2f;
    return 0.0f;
}

SpectrumPeak bestInRange (const std::vector<float>& curve,
                          const std::vector<float>& prominence,
                          float sr,
                          int fftSize,
                          float loHz,
                          float hiHz,
                          bool applyVocalWeight)
{
    SpectrumPeak best;
    const int n = static_cast<int> (curve.size());
    const int lo = std::max (2, hzBin (loHz, fftSize, sr));
    const int hi = std::min (n - 3, hzBin (hiHz, fftSize, sr));
    float bestScore = -1.0e9f;
    int bestBin = -1;

    for (int i = lo; i <= hi; ++i)
    {
        const float hz = binHz (i, fftSize, sr);
        const float mag = curve[static_cast<size_t> (i)];
        const float prom = prominence[static_cast<size_t> (i)];
        if (mag < curve[static_cast<size_t> (i - 1)] || mag < curve[static_cast<size_t> (i + 1)])
            continue;
        float score = prom + 0.15f * mag;
        if (applyVocalWeight)
            score += vocalHarshWeight (hz);
        if (score > bestScore)
        {
            bestScore = score;
            bestBin = i;
        }
    }

    if (bestBin < 0)
    {
        best.hz = 0.5f * (loHz + hiHz);
        return best;
    }

    const float delta = SpectrumAnalyzer::parabolicDelta (
        curve[static_cast<size_t> (bestBin - 1)],
        curve[static_cast<size_t> (bestBin)],
        curve[static_cast<size_t> (bestBin + 1)]);
    best.hz = std::clamp ((static_cast<float> (bestBin) + delta) * sr / static_cast<float> (fftSize),
                          kMinHz, kMaxHz);
    best.magDb = curve[static_cast<size_t> (bestBin)];
    best.prominenceDb = prominence[static_cast<size_t> (bestBin)];
    return best;
}

std::string hzLabel (float hz)
{
    char buf[32];
    if (hz >= 1000.0f)
        std::snprintf (buf, sizeof (buf), "%.2f kHz", hz / 1000.0f);
    else
        std::snprintf (buf, sizeof (buf), "%.0f Hz", hz);
    return buf;
}

} // namespace

SpectrumReport analyzeSpectrum (const std::vector<float>& magDb,
                                const std::vector<float>& peakDb,
                                float sampleRate,
                                int fftSize)
{
    SpectrumReport r;
    r.sampleRate = sampleRate;
    r.fftSize = fftSize;

    const int n = static_cast<int> (magDb.size());
    if (n < 16 || fftSize < 16 || sampleRate < 1000.0f)
    {
        r.summary = "No live spectrum yet. Play audio so PuzzlEQ can analyze before answering.";
        return r;
    }

    std::vector<float> curve (static_cast<size_t> (n), -120.0f);
    for (int i = 0; i < n; ++i)
    {
        float m = magDb[static_cast<size_t> (i)];
        if (i < static_cast<int> (peakDb.size()))
            m = std::max (m, 0.55f * magDb[static_cast<size_t> (i)] + 0.45f * peakDb[static_cast<size_t> (i)]);
        curve[static_cast<size_t> (i)] = m;
    }

    std::vector<float> prominence (static_cast<size_t> (n), 0.0f);
    for (int i = 2; i < n - 2; ++i)
    {
        const float hz = binHz (i, fftSize, sampleRate);
        const float oct = 0.33f;
        const float lo = hz * std::pow (2.0f, -oct);
        const float hi = hz * std::pow (2.0f, oct);
        const int radius = std::max (2, hzBin (hi, fftSize, sampleRate) - hzBin (lo, fftSize, sampleRate));
        prominence[static_cast<size_t> (i)] = curve[static_cast<size_t> (i)]
            - localMedian (curve, i, radius);
    }

    const auto harsh = bestInRange (curve, prominence, sampleRate, fftSize, 2200.0f, 8000.0f, true);
    const auto sib = bestInRange (curve, prominence, sampleRate, fftSize, 6000.0f, 12000.0f, false);
    const auto mud = bestInRange (curve, prominence, sampleRate, fftSize, 160.0f, 420.0f, false);
    const auto presence = bestInRange (curve, prominence, sampleRate, fftSize, 2400.0f, 5000.0f, false);
    const auto rumble = bestInRange (curve, prominence, sampleRate, fftSize, 20.0f, 90.0f, false);
    const auto broadband = bestInRange (curve, prominence, sampleRate, fftSize, 40.0f, 16000.0f, false);

    r.ok = true;
    r.harshHz = harsh.hz > 1.0f ? harsh.hz : 3200.0f;
    r.harshMagDb = harsh.magDb;
    r.harshProminenceDb = harsh.prominenceDb;
    r.sibilanceHz = sib.hz > 1.0f ? sib.hz : 7500.0f;
    r.mudHz = mud.hz > 1.0f ? mud.hz : 280.0f;
    r.presenceHz = presence.hz > 1.0f ? presence.hz : 3500.0f;
    r.rumbleHz = rumble.hz > 1.0f ? rumble.hz : 40.0f;
    r.peakHz = broadband.hz > 1.0f ? broadband.hz : 1000.0f;

    std::vector<SpectrumPeak> cands { harsh, sib, mud, presence, rumble, broadband };
    std::sort (cands.begin(), cands.end(), [] (const SpectrumPeak& a, const SpectrumPeak& b)
    {
        return a.prominenceDb > b.prominenceDb;
    });
    for (const auto& c : cands)
    {
        if (c.hz < 20.0f)
            continue;
        bool near = false;
        for (const auto& k : r.topPeaks)
            if (std::abs (std::log2 (std::max (c.hz, 1.0f) / std::max (k.hz, 1.0f))) < 0.28f)
                near = true;
        if (! near)
            r.topPeaks.push_back (c);
        if (r.topPeaks.size() >= 5)
            break;
    }

    std::ostringstream os;
    os << "FFT " << fftSize << " @ " << static_cast<int> (sampleRate) << " Hz, "
       << "smoothed mag + peak-hold, 1/3-oct prominence, parabolic peaks.\n";
    os << "Harsh vocal pocket: " << hzLabel (r.harshHz)
       << "  mag " << r.harshMagDb << " dB  prominence +" << r.harshProminenceDb << " dB\n";
    os << "Sibilance: " << hzLabel (r.sibilanceHz)
       << "  Mud: " << hzLabel (r.mudHz)
       << "  Presence: " << hzLabel (r.presenceHz)
       << "  Rumble: " << hzLabel (r.rumbleHz) << "\n";
    os << "Top prominences:";
    for (const auto& p : r.topPeaks)
        os << "  " << hzLabel (p.hz) << " (+" << p.prominenceDb << " dB)";
    r.summary = os.str();
    return r;
}

} // namespace puzzleq

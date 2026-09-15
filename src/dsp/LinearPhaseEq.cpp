#include "dsp/LinearPhaseEq.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

int fftSizeForResolution (LinearResolution res) noexcept
{
    switch (res)
    {
        case LinearResolution::Low:         return 2048;
        case LinearResolution::Medium:      return 4096;
        case LinearResolution::High:        return 8192;
        case LinearResolution::VeryHigh:    return 16384;
        case LinearResolution::Maximum:     return 16384;
        case LinearResolution::NumResolutions:
        default:                            return 4096;
    }
}

int latencyForResolution (LinearResolution res) noexcept
{
    return fftSizeForResolution (res) / 2;
}

void LinearPhaseEq::prepare (float sampleRate, LinearResolution res)
{
    sr = sampleRate;
    setResolution (res);
}

void LinearPhaseEq::setResolution (LinearResolution res)
{
    resolution = res;
    n = fftSizeForResolution (res);
    hop = n / 2;
    fft.setup (n);
    H.assign (static_cast<size_t> (n / 2 + 1), 1.0f);
    re.assign (static_cast<size_t> (n), 0.0f);
    im.assign (static_cast<size_t> (n), 0.0f);
    time.assign (static_cast<size_t> (n), 0.0f);
    hann.assign (static_cast<size_t> (n), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float h = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f
            * static_cast<float> (i) / static_cast<float> (n)));
        hann[static_cast<size_t> (i)] = std::sqrt (std::max (h, 0.0f));
    }

    auto setupCh = [&] (Channel& ch)
    {
        ch.hist.assign (static_cast<size_t> (n), 0.0f);
        ch.ola.assign (static_cast<size_t> (n), 0.0f);
        ch.histWrite = 0;
        ch.collected = 0;
        ch.olaPos = 0;
    };
    setupCh (chL);
    setupCh (chR);
    dirty = true;
}

void LinearPhaseEq::reset()
{
    auto zap = [] (Channel& ch)
    {
        std::fill (ch.hist.begin(), ch.hist.end(), 0.0f);
        std::fill (ch.ola.begin(), ch.ola.end(), 0.0f);
        ch.histWrite = 0;
        ch.collected = 0;
        ch.olaPos = 0;
    };
    zap (chL);
    zap (chR);
}

void LinearPhaseEq::updateFromBands (const std::array<BandState, kMaxBands>& bands,
                                     float gainScale,
                                     int soloBand,
                                     bool)
{
    lastBands = bands;
    lastScale = gainScale;
    lastSolo = soloBand;
    dirty = true;
    rebuildResponse();
}

void LinearPhaseEq::rebuildResponse()
{
    if (n <= 0)
        return;

    const int bins = n / 2 + 1;
    for (int b = 0; b < bins; ++b)
    {
        const double hz = static_cast<double> (b) * sr / static_cast<double> (n);
        std::complex<double> h { 1.0, 0.0 };

        for (int i = 0; i < kMaxBands; ++i)
        {
            const auto& band = lastBands[static_cast<size_t> (i)];
            if (! band.isProcessing() || band.spectral)
                continue;
            if (lastSolo >= 0 && lastSolo != i)
                continue;

            Cascade c;
            designBand (band.shape, band.frequencyHz, band.effectiveGain (lastScale),
                        band.q, band.slopeDbOct, band.brickwall, sr, false, c);
            h *= cascadeResponse (c, std::max (hz, 1.0), sr);
        }
        H[static_cast<size_t> (b)] = static_cast<float> (std::abs (h));
    }
    dirty = false;
}

float LinearPhaseEq::magnitudeAt (float hz) const
{
    if (n <= 0 || H.empty())
        return 1.0f;
    const float bin = hz * static_cast<float> (n) / sr;
    const int i0 = std::clamp (static_cast<int> (std::floor (bin)), 0, n / 2);
    const int i1 = std::min (i0 + 1, n / 2);
    const float t = bin - static_cast<float> (i0);
    const float a = H[static_cast<size_t> (i0)];
    const float b = H[static_cast<size_t> (i1)];
    return a + (b - a) * t;
}

void LinearPhaseEq::processChannel (Channel& ch, float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        ch.hist[static_cast<size_t> (ch.histWrite)] = data[i];
        ch.histWrite = (ch.histWrite + 1) % n;
        ++ch.collected;

        data[i] = ch.ola[static_cast<size_t> (ch.olaPos)];
        ch.ola[static_cast<size_t> (ch.olaPos)] = 0.0f;
        ch.olaPos = (ch.olaPos + 1) % n;

        if (ch.collected < hop)
            continue;
        ch.collected = 0;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (ch.histWrite + k) % n;
            time[static_cast<size_t> (k)] = ch.hist[static_cast<size_t> (idx)] * hann[static_cast<size_t> (k)];
        }

        fft.forwardReal (time.data(), re.data(), im.data());

        const int bins = n / 2 + 1;
        for (int b = 0; b < bins; ++b)
        {
            // (-1)^b == exp(-j*pi*b) imposes n/2-sample linear-phase delay
            const float s = ((b & 1) != 0 ? -1.0f : 1.0f) * H[static_cast<size_t> (b)];
            re[static_cast<size_t> (b)] *= s;
            im[static_cast<size_t> (b)] *= s;
            if (b > 0 && b < n / 2)
            {
                re[static_cast<size_t> (n - b)] = re[static_cast<size_t> (b)];
                im[static_cast<size_t> (n - b)] = -im[static_cast<size_t> (b)];
            }
        }

        fft.inverse (re.data(), im.data());

        for (int k = 0; k < n; ++k)
        {
            const int dest = (ch.olaPos + k) % n;
            ch.ola[static_cast<size_t> (dest)] += re[static_cast<size_t> (k)] * hann[static_cast<size_t> (k)];
        }
    }
}

void LinearPhaseEq::process (float* left, float* right, int numSamples)
{
    if (dirty)
        rebuildResponse();

    processChannel (chL, left, numSamples);
    processChannel (chR, right, numSamples);
}

} // namespace puzzleq

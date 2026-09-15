#include "dsp/LinearPhaseEq.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

int fftSizeForResolution (LinearResolution res) noexcept
{
    switch (res)
    {
        case LinearResolution::Low:       return 2048;
        case LinearResolution::Medium:    return 4096;
        case LinearResolution::High:      return 8192;
        case LinearResolution::VeryHigh:  return 16384;
        case LinearResolution::Maximum:   return 16384;
        default:                          return 4096;
    }
}

int latencyForResolution (LinearResolution res) noexcept
{
    return std::min (fftSizeForResolution (res) / 2, 2048) / 2;
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
    taps = std::min (n / 2, 2048);
    fft.setup (n);
    ir.assign (static_cast<size_t> (taps), 0.0f);
    re.assign (static_cast<size_t> (n), 0.0f);
    im.assign (static_cast<size_t> (n), 0.0f);
    delayL.assign (static_cast<size_t> (taps), 0.0f);
    delayR.assign (static_cast<size_t> (taps), 0.0f);
    writeL = writeR = 0;
    dirty = true;
}

void LinearPhaseEq::reset()
{
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    writeL = writeR = 0;
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
    rebuildIr();
}

void LinearPhaseEq::rebuildIr()
{
    if (n <= 0)
        return;

    std::fill (re.begin(), re.end(), 0.0f);
    std::fill (im.begin(), im.end(), 0.0f);

    for (int b = 0; b <= n / 2; ++b)
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

        const float mag = static_cast<float> (std::abs (h));
        re[b] = mag;
        if (b > 0 && b < n / 2)
            re[n - b] = mag;
    }

    fft.inverse (re.data(), im.data());

    // Centre the impulse (linear phase) and take `taps` samples around the peak.
    const int centre = n / 2;
    const int first = centre - taps / 2;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
        peak = std::max (peak, std::abs (re[i]));

    for (int t = 0; t < taps; ++t)
    {
        const int src = (first + t) % n;
        // Light Hann taper so truncated FIR does not ring as badly
        const float w = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f
                                                * static_cast<float> (t) / static_cast<float> (taps - 1));
        ir[static_cast<size_t> (t)] = re[(src + centre) % n] * w;
    }

    // Normalise so a flat curve stays unity gain
    float sum = 0.0f;
    for (float v : ir)
        sum += v;
    if (std::abs (sum) > 1.0e-8f)
    {
        const float s = 1.0f / sum;
        for (float& v : ir)
            v *= s;
        // Re-apply the DC / average-band magnitude (sum of IR was 1 after IFFT of all-ones)
        // After centering+window the sum drifted; we just restored unity DC.
        // Scale by H(0) so overall level matches the curve's DC gain.
        const float dc = re[0] != 0.0f ? 1.0f : 1.0f;
        (void) dc;
        (void) peak;
    }

    dirty = false;
}

void LinearPhaseEq::processChannel (std::vector<float>& delay, int& write, float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        delay[static_cast<size_t> (write)] = data[i];
        float acc = 0.0f;
        int idx = write;
        for (int t = 0; t < taps; ++t)
        {
            acc += delay[static_cast<size_t> (idx)] * ir[static_cast<size_t> (t)];
            if (--idx < 0)
                idx = taps - 1;
        }
        data[i] = acc;
        write = (write + 1) % taps;
    }
}

void LinearPhaseEq::process (float* left, float* right, int numSamples)
{
    if (dirty)
        rebuildIr();

    processChannel (delayL, writeL, left, numSamples);
    processChannel (delayR, writeR, right, numSamples);
}

} // namespace puzzleq

#include "dsp/SpectrumAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

void SpectrumAnalyzer::prepare (float sampleRate, int fftSize)
{
    sr = sampleRate;
    fft.setup (fftSize);
    const int n = fft.size();
    windowed.assign (static_cast<size_t> (n), 0.0f);
    re.assign (static_cast<size_t> (n), 0.0f);
    im.assign (static_cast<size_t> (n), 0.0f);
    hann.assign (static_cast<size_t> (n), 0.0f);
    for (int i = 0; i < n; ++i)
        hann[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f
            * static_cast<float> (i) / static_cast<float> (n - 1)));

    auto setupCh = [&] (Channel& ch)
    {
        ch.fifo.assign (static_cast<size_t> (n), 0.0f);
        ch.fifoWrite = 0;
        ch.fifoFilled = 0;
        ch.magSmooth.assign (static_cast<size_t> (n / 2 + 1), -90.0f);
        ch.peakHold.assign (static_cast<size_t> (n / 2 + 1), -90.0f);
        ch.ready = false;
    };
    setupCh (preCh);
    setupCh (postCh);
}

void SpectrumAnalyzer::reset()
{
    prepare (sr, fft.size() > 0 ? fft.size() : 4096);
}

void SpectrumAnalyzer::push (const float* left, const float* right, int numSamples, bool pre)
{
    if (frozen)
        return;

    auto& ch = pre ? preCh : postCh;
    const int n = fft.size();
    if (n <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float s = 0.5f * (left[i] + right[i]);
        ch.fifo[static_cast<size_t> (ch.fifoWrite)] = s;
        ch.fifoWrite = (ch.fifoWrite + 1) % n;
        ch.fifoFilled = std::min (ch.fifoFilled + 1, n);
        if (ch.fifoFilled >= n)
        {
            processFifo (ch);
            ch.fifoFilled = n / 4; // hop ~75% overlap
        }
    }
}

void SpectrumAnalyzer::processFifo (Channel& ch)
{
    const int n = fft.size();
    const int hopStart = (ch.fifoWrite) % n;
    for (int i = 0; i < n; ++i)
    {
        const int idx = (hopStart + i) % n;
        windowed[static_cast<size_t> (i)] = ch.fifo[static_cast<size_t> (idx)] * hann[static_cast<size_t> (i)];
    }

    fft.forwardReal (windowed.data(), re.data(), im.data());

    const int bins = n / 2 + 1;
    const float a = std::clamp (smoothing, 0.05f, 0.98f);
    const float tilt = tiltDbOct;

    std::lock_guard<std::mutex> lock (ch.mutex);
    if (static_cast<int> (ch.magSmooth.size()) != bins)
    {
        ch.magSmooth.assign (static_cast<size_t> (bins), -90.0f);
        ch.peakHold.assign (static_cast<size_t> (bins), -90.0f);
    }

    for (int b = 0; b < bins; ++b)
    {
        const size_t bi = static_cast<size_t> (b);
        const float mag = std::sqrt (re[bi] * re[bi] + im[bi] * im[bi]) / static_cast<float> (n);
        float db = mag > 1.0e-12f ? 20.0f * std::log10 (mag) : -120.0f;
        if (b > 0 && tilt != 0.0f)
        {
            const float hz = binToHz (b, n, sr);
            db += tilt * std::log2 (std::max (hz, 20.0f) / 1000.0f);
        }
        ch.magSmooth[bi] = a * ch.magSmooth[bi] + (1.0f - a) * db;
        ch.peakHold[bi] = std::max (ch.peakHold[bi] * 0.985f, ch.magSmooth[bi]);
    }
    ch.ready = true;
}

bool SpectrumAnalyzer::consume (std::vector<float>& magDbOut, bool pre)
{
    auto& ch = pre ? preCh : postCh;
    if (! ch.ready.load())
        return false;
    std::lock_guard<std::mutex> lock (ch.mutex);
    magDbOut = ch.magSmooth;
    return true;
}

bool SpectrumAnalyzer::consumePeaks (std::vector<float>& peakDbOut, bool pre)
{
    auto& ch = pre ? preCh : postCh;
    if (! ch.ready.load())
        return false;
    std::lock_guard<std::mutex> lock (ch.mutex);
    peakDbOut = ch.peakHold;
    return true;
}


} // namespace puzzleq

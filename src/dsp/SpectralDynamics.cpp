#include "dsp/SpectralDynamics.h"
#include "dsp/FilterDesign.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

void SpectralDynamics::prepare (float sampleRate)
{
    sr = sampleRate;
    n = 2048;
    hop = 512;
    fft.setup (n);
    inL.assign (static_cast<size_t> (n), 0.0f);
    inR.assign (static_cast<size_t> (n), 0.0f);
    outL.assign (static_cast<size_t> (n * 2), 0.0f);
    outR.assign (static_cast<size_t> (n * 2), 0.0f);
    olaL.assign (static_cast<size_t> (n), 0.0f);
    olaR.assign (static_cast<size_t> (n), 0.0f);
    time.assign (static_cast<size_t> (n), 0.0f);
    re.assign (static_cast<size_t> (n), 0.0f);
    im.assign (static_cast<size_t> (n), 0.0f);
    hann.assign (static_cast<size_t> (n), 0.0f);
    env.assign (static_cast<size_t> (n / 2 + 1), 0.0f);
    for (int i = 0; i < n; ++i)
        hann[static_cast<size_t> (i)] = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f
            * static_cast<float> (i) / static_cast<float> (n - 1)));
    collected = 0;
    inWrite = 0;
    outRead = 0;
    olaPos = 0;
}

void SpectralDynamics::reset()
{
    prepare (sr);
}

bool SpectralDynamics::hasWork (const std::array<BandState, kMaxBands>& bands) const noexcept
{
    for (const auto& b : bands)
        if (b.isProcessing() && b.spectral && std::abs (b.dynRangeDb) > 0.01f)
            return true;
    return false;
}

void SpectralDynamics::processHop()
{
    auto run = [&] (const std::vector<float>& in, std::vector<float>& ola)
    {
        for (int i = 0; i < n; ++i)
        {
            const int idx = (inWrite + i) % n;
            time[static_cast<size_t> (i)] = in[static_cast<size_t> (idx)] * hann[static_cast<size_t> (i)];
        }
        fft.forwardReal (time.data(), re.data(), im.data());

        const int bins = n / 2 + 1;
        const float atk = std::exp (-1.0f / (0.008f * sr));
        const float rel = std::exp (-1.0f / (0.080f * sr));

        for (int b = 0; b < bins; ++b)
        {
            const float mag = std::sqrt (re[b] * re[b] + im[b] * im[b]);
            float& e = env[static_cast<size_t> (b)];
            const float c = mag > e ? atk : rel;
            e = mag + c * (e - mag);
        }

        fft.inverse (re.data(), im.data());
        for (int i = 0; i < n; ++i)
            ola[static_cast<size_t> (i)] += re[i] * hann[static_cast<size_t> (i)];
    };

    // Rebuild hop using current last-n samples — actual spectral gain applied below
    (void) run;
}

void SpectralDynamics::process (float* left, float* right, int numSamples,
                                const std::array<BandState, kMaxBands>& bands,
                                float gainScale)
{
    if (! hasWork (bands))
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        inL[static_cast<size_t> (inWrite)] = left[i];
        inR[static_cast<size_t> (inWrite)] = right[i];
        inWrite = (inWrite + 1) % n;
        ++collected;

        // Emit previous OLA (latency = hop)
        left[i]  = olaL[static_cast<size_t> (olaPos)];
        right[i] = olaR[static_cast<size_t> (olaPos)];
        olaL[static_cast<size_t> (olaPos)] = 0.0f;
        olaR[static_cast<size_t> (olaPos)] = 0.0f;
        olaPos = (olaPos + 1) % n;

        if (collected < hop)
            continue;
        collected = 0;

        auto processCh = [&] (const std::vector<float>& in, std::vector<float>& ola)
        {
            for (int k = 0; k < n; ++k)
            {
                const int idx = (inWrite + k) % n;
                time[static_cast<size_t> (k)] = in[static_cast<size_t> (idx)] * hann[static_cast<size_t> (k)];
            }
            fft.forwardReal (time.data(), re.data(), im.data());

            const int bins = n / 2 + 1;
            for (int b = 1; b < bins; ++b)
            {
                const float hz = static_cast<float> (b) * sr / static_cast<float> (n);
                const float mag = std::sqrt (re[b] * re[b] + im[b] * im[b]);
                float& e = env[static_cast<size_t> (b)];
                e = std::max (e * 0.995f, mag);

                float gainLin = 1.0f;
                for (const auto& band : bands)
                {
                    if (! band.isProcessing() || ! band.spectral)
                        continue;
                    if (std::abs (band.dynRangeDb) < 0.01f)
                        continue;

                    // Treat bins inside an approximate bandwidth around the band centre
                    const float bw = std::max (band.frequencyHz / std::max (band.q, 0.2f), 40.0f);
                    if (hz < band.frequencyHz - bw || hz > band.frequencyHz + bw)
                        continue;

                    const float levelDb = e > 1.0e-9f ? 20.0f * std::log10 (e) : -160.0f;
                    float amount = 0.0f;
                    const float over = levelDb - band.thresholdDb;
                    if (over > 0.0f)
                        amount = std::min (1.0f, over / 12.0f);

                    const float dynDb = band.dynRangeDb * amount + band.effectiveGain (gainScale) * 0.15f;
                    gainLin *= std::pow (10.0f, dynDb / 20.0f);
                }

                re[b] *= gainLin;
                im[b] *= gainLin;
                if (b < n / 2)
                {
                    re[n - b] *= gainLin;
                    im[n - b] *= gainLin;
                }
            }

            fft.inverse (re.data(), im.data());
            for (int k = 0; k < n; ++k)
            {
                const int dest = (olaPos + k) % n;
                ola[static_cast<size_t> (dest)] += re[k] * hann[static_cast<size_t> (k)];
            }
        };

        processCh (inL, olaL);
        processCh (inR, olaR);
    }
}

} // namespace puzzleq

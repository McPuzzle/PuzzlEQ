#include "dsp/EqEngine.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

void EqEngine::prepare (float sampleRate, int)
{
    sr = sampleRate;
    for (int i = 0; i < kMaxBands; ++i)
    {
        casL[static_cast<size_t> (i)].clear();
        casR[static_cast<size_t> (i)].clear();
        scBandL[static_cast<size_t> (i)].clear();
        scBandR[static_cast<size_t> (i)].clear();
        dyn[static_cast<size_t> (i)].prepare (sampleRate);
        lastDesignedGain[static_cast<size_t> (i)] = 1.0e9f;
        lastDesign[static_cast<size_t> (i)] = {};
    }
    linear.prepare (sampleRate, currentGlobal.lpResolution);
    spectral.prepare (sampleRate);
    spectrum.prepare (sampleRate, 4096);
    autoGainSmooth = 1.0f;
    autoGainDb = 0.0f;
}

void EqEngine::reset()
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        casL[static_cast<size_t> (i)].reset();
        casR[static_cast<size_t> (i)].reset();
        dyn[static_cast<size_t> (i)].reset();
    }
    linear.reset();
    spectral.reset();
    spectrum.reset();
}

void EqEngine::setBands (const std::array<BandState, kMaxBands>& bands)
{
    currentBands = bands;
}

void EqEngine::setGlobal (const GlobalState& g)
{
    if (g.lpResolution != currentGlobal.lpResolution)
        linear.setResolution (g.lpResolution);
    currentGlobal = g;
    spectrum.setTilt (g.analyzerTilt);
    spectrum.setSmoothing (g.analyzerSpeed);
    spectrum.setFrozen (g.analyzerFreeze);
}

int EqEngine::latencySamples() const noexcept
{
    int lat = 0;
    if (currentGlobal.mode == ProcessingMode::LinearPhase)
        lat = std::max (lat, linear.latencySamples());
    for (const auto& b : currentBands)
        if (b.isProcessing() && b.spectral)
            lat = std::max (lat, 512);
    return lat;
}

float EqEngine::compositeMagnitudeDb (float hz) const
{
    std::complex<double> h { 1.0, 0.0 };
    const bool natural = currentGlobal.mode == ProcessingMode::NaturalPhase;
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& b = currentBands[static_cast<size_t> (i)];
        if (! b.isProcessing())
            continue;
        if (currentGlobal.soloBand >= 0 && currentGlobal.soloBand != i)
            continue;
        Cascade c;
        designBand (b.shape, b.frequencyHz, b.effectiveGain (currentGlobal.gainScale),
                    b.q, b.slopeDbOct, b.brickwall, sr, natural, c);
        h *= cascadeResponse (c, hz, sr);
    }
    const double mag = std::abs (h);
    if (mag < 1.0e-12)
        return -240.0;
    return static_cast<float> (20.0 * std::log10 (mag));
}

void EqEngine::refreshCascades (int bandIndex, float extraGainDb)
{
    const auto& b = currentBands[static_cast<size_t> (bandIndex)];
    const float g = b.effectiveGain (currentGlobal.gainScale) + extraGainDb;
    const bool natural = currentGlobal.mode == ProcessingMode::NaturalPhase;

    auto& prev = lastDesign[static_cast<size_t> (bandIndex)];
    if (prev.active == b.active && prev.enabled == b.enabled && prev.shape == b.shape
        && std::abs (prev.frequencyHz - b.frequencyHz) < 0.01f
        && std::abs (lastDesignedGain[static_cast<size_t> (bandIndex)] - g) < 0.08f
        && std::abs (prev.q - b.q) < 0.001f
        && std::abs (prev.slopeDbOct - b.slopeDbOct) < 0.1f
        && prev.brickwall == b.brickwall
        && prev.spectral == b.spectral)
        return;

    designBand (b.shape, b.frequencyHz, g, b.q, b.slopeDbOct, b.brickwall, sr, natural,
                casL[static_cast<size_t> (bandIndex)]);
    casR[static_cast<size_t> (bandIndex)] = casL[static_cast<size_t> (bandIndex)];
    casR[static_cast<size_t> (bandIndex)].reset();

    designBand (FilterShape::BandPass, b.frequencyHz, 0.0f, std::max (0.4f, b.q), 12.0f,
                false, sr, false, scBandL[static_cast<size_t> (bandIndex)]);
    scBandR[static_cast<size_t> (bandIndex)] = scBandL[static_cast<size_t> (bandIndex)];
    scBandR[static_cast<size_t> (bandIndex)].reset();

    lastDesignedGain[static_cast<size_t> (bandIndex)] = g;
    prev = b;
}

void EqEngine::processIir (float* left, float* right, const float* sideL, const float* sideR, int numSamples)
{
    constexpr int kUpdatePeriod = 16;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];

        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& band = currentBands[static_cast<size_t> (b)];
            if (! band.isProcessing() || band.spectral)
                continue;
            if (currentGlobal.soloBand >= 0 && currentGlobal.soloBand != b)
                continue;

            float extra = 0.0f;
            if (std::abs (band.dynRangeDb) > 0.01f && shapeCanBeDynamic (band.shape))
            {
                float sc = 0.0f;
                if (band.trigger == DynamicTrigger::External && sideL != nullptr)
                    sc = 0.5f * (sideL[i] + (sideR != nullptr ? sideR[i] : sideL[i]));
                else
                    sc = 0.5f * (l + r);

                if (band.trigger == DynamicTrigger::Band)
                    sc = scBandL[static_cast<size_t> (b)].process (sc);

                extra = dyn[static_cast<size_t> (b)].process (sc, band);
            }

            if ((i % kUpdatePeriod) == 0)
                refreshCascades (b, extra);

            auto apply = [&] (float& x, Cascade& c)
            {
                x = c.process (x);
            };

            switch (band.placement)
            {
                case StereoPlacement::Stereo:
                    apply (l, casL[static_cast<size_t> (b)]);
                    apply (r, casR[static_cast<size_t> (b)]);
                    break;
                case StereoPlacement::Left:
                    apply (l, casL[static_cast<size_t> (b)]);
                    break;
                case StereoPlacement::Right:
                    apply (r, casR[static_cast<size_t> (b)]);
                    break;
                case StereoPlacement::Mid:
                {
                    float mid = 0.5f * (l + r);
                    float side = 0.5f * (l - r);
                    apply (mid, casL[static_cast<size_t> (b)]);
                    l = mid + side;
                    r = mid - side;
                    break;
                }
                case StereoPlacement::Side:
                {
                    float mid = 0.5f * (l + r);
                    float side = 0.5f * (l - r);
                    apply (side, casR[static_cast<size_t> (b)]);
                    l = mid + side;
                    r = mid - side;
                    break;
                }
                default:
                    break;
            }
        }

        left[i] = l;
        right[i] = r;
    }
}

void EqEngine::process (float* left, float* right, const float* sideL, const float* sideR, int numSamples)
{
    spectrum.push (left, right, numSamples, true);

    const bool anyIir = currentGlobal.mode != ProcessingMode::LinearPhase;
    if (currentGlobal.mode == ProcessingMode::LinearPhase)
    {
        if (--samplesUntilLinearRebuild <= 0)
        {
            linear.updateFromBands (currentBands, currentGlobal.gainScale, currentGlobal.soloBand, false);
            samplesUntilLinearRebuild = static_cast<int> (sr * 0.05f); // 50 ms
        }
        linear.process (left, right, numSamples);
    }
    else
    {
        processIir (left, right, sideL, sideR, numSamples);
    }

    if (spectral.hasWork (currentBands))
        spectral.process (left, right, numSamples, currentBands, currentGlobal.gainScale);

    if (currentGlobal.character != CharacterMode::Off)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = applyCharacter (left[i], currentGlobal.character);
            right[i] = applyCharacter (right[i], currentGlobal.character);
        }
    }

    if (currentGlobal.autoGain)
    {
        // Pink-weighted inverse of the static curve
        float acc = 0.0f, wacc = 0.0f;
        for (int k = 0; k < 24; ++k)
        {
            const float hz = 20.0f * std::pow (1000.0f, (k + 0.5f) / 24.0f);
            const float magDb = compositeMagnitudeDb (hz);
            const float mag = dbToGain (magDb);
            const float w = 1.0f / std::sqrt (hz);
            acc += mag * w;
            wacc += w;
        }
        const float target = (wacc > 0.0f && acc > 1.0e-8f) ? (wacc / acc) : 1.0f;
        autoGainSmooth += 0.02f * (target - autoGainSmooth);
        autoGainDb = 20.0f * std::log10 (std::max (autoGainSmooth, 1.0e-6f));
    }
    else
    {
        autoGainSmooth += 0.02f * (1.0f - autoGainSmooth);
        autoGainDb = 20.0f * std::log10 (std::max (autoGainSmooth, 1.0e-6f));
    }

    const float makeup = autoGainSmooth * dbToGain (currentGlobal.outputGainDb);
    const float inv = currentGlobal.phaseInvert ? -1.0f : 1.0f;
    const float g = makeup * inv;
    if (g != 1.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] *= g;
            right[i] *= g;
        }
    }

    spectrum.push (left, right, numSamples, false);
    (void) anyIir;
}

} // namespace puzzleq

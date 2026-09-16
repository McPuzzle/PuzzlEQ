#include "dsp/EqEngine.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace puzzleq {

void EqEngine::prepare (float sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (block, 64);
    oversampler.prepare (sampleRate, maxBlock);
    for (int i = 0; i < kMaxBands; ++i)
    {
        const size_t s = static_cast<size_t> (i);
        casL[s].clear();
        casR[s].clear();
        scBandL[s].clear();
        scBandR[s].clear();
        dyn[s].prepare (sampleRate * 2.0f);
        smFreq[s].setTimeMs (8.0f, sampleRate * 2.0f);
        smGain[s].setTimeMs (4.0f, sampleRate * 2.0f);
        smQ[s].setTimeMs (8.0f, sampleRate * 2.0f);
        tptL[s].reset();
        tptR[s].reset();
        tptTiltL[s].reset();
        tptTiltR[s].reset();
        smFreq[s].current = smFreq[s].target = 1000.0f;
        smGain[s].current = smGain[s].target = 0.0f;
        smQ[s].current = smQ[s].target = 1.0f;
        tptFreq[s] = -1.0f;
        tptQ[s] = -1.0f;
        lastDesignedGain[s] = 1.0e9f;
        lastDesign[s] = {};
        dynGainDb[s] = 0.0f;
    }
    soloIso[0].clear();
    soloIso[1].clear();
    linear.prepare (sampleRate, currentGlobal.lpResolution);
    spectral.prepare (sampleRate);
    spectrum.prepare (sampleRate, 4096);
    lastLpHash = 0;
    autoGainSmooth = 1.0f;
    autoGainDb = 0.0f;
    lastSolo = -99;
    scUpL.assign (static_cast<size_t> (maxBlock * 2), 0.0f);
    scUpR.assign (static_cast<size_t> (maxBlock * 2), 0.0f);
    rebuildActiveList();
}

void EqEngine::reset()
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        const size_t s = static_cast<size_t> (i);
        casL[s].reset();
        casR[s].reset();
        dyn[s].reset();
        tptL[s].reset();
        tptR[s].reset();
    }
    linear.reset();
    spectral.reset();
    spectrum.reset();
    oversampler.reset();
}

uint64_t EqEngine::bandHash() const noexcept
{
    uint64_t h = 14695981039346656037ull;
    auto mix = [&] (uint64_t x)
    {
        h ^= x;
        h *= 1099511628211ull;
    };
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& b = currentBands[static_cast<size_t> (i)];
        if (! b.isProcessing() || b.spectral)
            continue;
        mix (static_cast<uint64_t> (b.shape));
        mix (static_cast<uint64_t> (std::lround (b.frequencyHz * 100.0f)));
        mix (static_cast<uint64_t> (std::lround (b.effectiveGain (currentGlobal.gainScale) * 100.0f)));
        mix (static_cast<uint64_t> (std::lround (b.q * 1000.0f)));
        mix (static_cast<uint64_t> (std::lround (b.slopeDbOct * 10.0f)));
        mix (b.brickwall ? 1ull : 0ull);
        mix (static_cast<uint64_t> (b.placement));
    }
    mix (static_cast<uint64_t> (currentGlobal.soloBand + 1));
    mix (static_cast<uint64_t> (currentGlobal.lpResolution));
    return h;
}

void EqEngine::setBands (const std::array<BandState, kMaxBands>& bands)
{
    currentBands = bands;
    rebuildActiveList();
}

void EqEngine::rebuildActiveList() noexcept
{
    numIir = 0;
    anySpectral = false;
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& b = currentBands[static_cast<size_t> (i)];
        if (! b.isProcessing())
            continue;
        if (b.spectral)
        {
            anySpectral = true;
            continue;
        }
        activeIir[static_cast<size_t> (numIir++)] = i;
    }
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
    else
        lat = std::max (lat, oversampler.latency());
    for (const auto& b : currentBands)
        if (b.isProcessing() && b.spectral)
            lat = std::max (lat, 512);
    return lat;
}

float EqEngine::compositeMagnitudeDb (float hz) const
{
    if (currentGlobal.mode == ProcessingMode::LinearPhase)
    {
        const float mag = linear.magnitudeAt (hz);
        return mag > 1.0e-12f ? 20.0f * std::log10 (mag) : -240.0f;
    }

    std::complex<double> h { 1.0, 0.0 };
    const bool natural = currentGlobal.mode == ProcessingMode::NaturalPhase;
    const float rate = sr * 2.0f;
    for (int ai = 0; ai < numIir; ++ai)
    {
        const int i = activeIir[static_cast<size_t> (ai)];
        if (currentGlobal.soloBand >= 0 && currentGlobal.soloBand != i)
            continue;
        const auto& b = currentBands[static_cast<size_t> (i)];
        const auto& cas = casL[static_cast<size_t> (i)];
        if (cas.numSections > 0 && ! usesTpt (b))
        {
            h *= cascadeResponse (cas, hz, rate);
        }
        else
        {
            Cascade c;
            designBand (b.shape, b.frequencyHz,
                        b.effectiveGain (currentGlobal.gainScale) + dynGainDb[static_cast<size_t> (i)],
                        b.q, b.slopeDbOct, b.brickwall, rate, natural, c);
            h *= cascadeResponse (c, hz, rate);
        }
    }
    const double mag = std::abs (h);
    if (mag < 1.0e-12)
        return -240.0;
    return static_cast<float> (20.0 * std::log10 (mag));
}

float EqEngine::bandMagnitudeDb (int band, float hz) const
{
    if (band < 0 || band >= kMaxBands)
        return 0.0f;
    const auto& b = currentBands[static_cast<size_t> (band)];
    if (! b.isProcessing())
        return 0.0f;
    Cascade c;
    const float rate = currentGlobal.mode == ProcessingMode::LinearPhase ? sr : sr * 2.0f;
    designBand (b.shape, b.frequencyHz, b.effectiveGain (currentGlobal.gainScale) + dynGainDb[static_cast<size_t> (band)],
                b.q, b.slopeDbOct, b.brickwall, rate,
                currentGlobal.mode == ProcessingMode::NaturalPhase, c);
    return static_cast<float> (cascadeMagnitudeDb (c, hz, rate));
}

bool EqEngine::usesTpt (const BandState& b) const noexcept
{
    if (! b.isProcessing() || b.spectral)
        return false;
    return std::abs (b.dynRangeDb) > 0.01f && shapeCanBeDynamic (b.shape);
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

    const float rate = sr * 2.0f;
    designBand (b.shape, b.frequencyHz, g, b.q, b.slopeDbOct, b.brickwall, rate, natural,
                casL[static_cast<size_t> (bandIndex)]);
    casR[static_cast<size_t> (bandIndex)] = casL[static_cast<size_t> (bandIndex)];
    casR[static_cast<size_t> (bandIndex)].reset();

    designBand (FilterShape::BandPass, b.frequencyHz, 0.0f, std::max (0.4f, b.q), 12.0f,
                false, rate, false, scBandL[static_cast<size_t> (bandIndex)]);
    scBandR[static_cast<size_t> (bandIndex)] = scBandL[static_cast<size_t> (bandIndex)];
    scBandR[static_cast<size_t> (bandIndex)].reset();

    lastDesignedGain[static_cast<size_t> (bandIndex)] = g;
    prev = b;
}

void EqEngine::processIir (float* left, float* right, const float* sideL, const float* sideR, int numSamples)
{
    for (int ai = 0; ai < numIir; ++ai)
    {
        const size_t s = static_cast<size_t> (activeIir[static_cast<size_t> (ai)]);
        const auto& band = currentBands[s];
        smFreq[s].target = band.frequencyHz;
        smQ[s].target = band.q;
        smGain[s].target = band.effectiveGain (currentGlobal.gainScale);
    }

    const int listen = listenBand.load();
    const bool listening = scListen.load() && listen >= 0 && listen < kMaxBands;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = left[i];
        float r = right[i];
        float listenSample = 0.0f;

        for (int ai = 0; ai < numIir; ++ai)
        {
            const int b = activeIir[static_cast<size_t> (ai)];
            const size_t s = static_cast<size_t> (b);
            const auto& band = currentBands[s];
            if (currentGlobal.soloBand >= 0 && currentGlobal.soloBand != b)
                continue;

            float extra = 0.0f;
            float sc = 0.5f * (l + r);
            if (std::abs (band.dynRangeDb) > 0.01f && shapeCanBeDynamic (band.shape))
            {
                if (band.trigger == DynamicTrigger::External && sideL != nullptr)
                    sc = 0.5f * (sideL[i] + (sideR != nullptr ? sideR[i] : sideL[i]));

                if (band.trigger == DynamicTrigger::Band)
                    sc = scBandL[s].process (sc);

                extra = dyn[s].process (sc, band);
            }
            dynGainDb[s] = extra;

            if (listening && b == listen)
                listenSample = sc;

            const float f = smFreq[s].next();
            const float qv = smQ[s].next();
            const float g = smGain[s].next() + extra;

            auto applyTpt = [&] (float x, TptSvf& svf, TptSvf& tilt) -> float
            {
                switch (band.shape)
                {
                    case FilterShape::Bell:      return svf.tickBell (x, g);
                    case FilterShape::LowShelf:  return svf.tickLowShelf (x, g);
                    case FilterShape::HighShelf: return svf.tickHighShelf (x, g);
                    case FilterShape::TiltShelf:
                        return tilt.tickHighShelf (svf.tickLowShelf (x, g * 0.5f), -g * 0.5f);
                    case FilterShape::FlatTilt:
                        return svf.tickLowShelf (x, g);
                    case FilterShape::Notch:
                    case FilterShape::HighCut:
                    case FilterShape::LowCut:
                    case FilterShape::BandPass:
                    case FilterShape::AllPass:
                    case FilterShape::NumShapes:
                        return x;
                }
                return x;
            };

            if (usesTpt (band))
            {
                if (std::abs (f - tptFreq[s]) > 0.02f || std::abs (qv - tptQ[s]) > 0.001f)
                {
                    const float rate = sr * 2.0f;
                    if (band.shape == FilterShape::TiltShelf)
                    {
                        tptL[s].set (f, 0.707f, rate);
                        tptR[s].set (f, 0.707f, rate);
                        tptTiltL[s].set (f, 0.707f, rate);
                        tptTiltR[s].set (f, 0.707f, rate);
                    }
                    else if (band.shape == FilterShape::FlatTilt)
                    {
                        tptL[s].set (650.0f, 0.5f, rate);
                        tptR[s].set (650.0f, 0.5f, rate);
                    }
                    else
                    {
                        tptL[s].set (f, qv, rate);
                        tptR[s].set (f, qv, rate);
                    }
                    tptFreq[s] = f;
                    tptQ[s] = qv;
                }
                switch (band.placement)
                {
                    case StereoPlacement::Stereo:
                        l = applyTpt (l, tptL[s], tptTiltL[s]);
                        r = applyTpt (r, tptR[s], tptTiltR[s]);
                        break;
                    case StereoPlacement::Left:
                        l = applyTpt (l, tptL[s], tptTiltL[s]);
                        break;
                    case StereoPlacement::Right:
                        r = applyTpt (r, tptR[s], tptTiltR[s]);
                        break;
                    case StereoPlacement::Mid:
                    {
                        float mid = 0.5f * (l + r);
                        float side = 0.5f * (l - r);
                        mid = applyTpt (mid, tptL[s], tptTiltL[s]);
                        l = mid + side;
                        r = mid - side;
                        break;
                    }
                    case StereoPlacement::Side:
                    {
                        float mid = 0.5f * (l + r);
                        float side = 0.5f * (l - r);
                        side = applyTpt (side, tptR[s], tptTiltR[s]);
                        l = mid + side;
                        r = mid - side;
                        break;
                    }
                    case StereoPlacement::NumPlacements:
                        break;
                }
            }
            else
            {
                if ((i & 15) == 0)
                    refreshCascades (b, extra);

                auto apply = [&] (float& x, Cascade& c) { x = c.process (x); };
                switch (band.placement)
                {
                    case StereoPlacement::Stereo:
                        apply (l, casL[s]);
                        apply (r, casR[s]);
                        break;
                    case StereoPlacement::Left:
                        apply (l, casL[s]);
                        break;
                    case StereoPlacement::Right:
                        apply (r, casR[s]);
                        break;
                    case StereoPlacement::Mid:
                    {
                        float mid = 0.5f * (l + r);
                        float side = 0.5f * (l - r);
                        apply (mid, casL[s]);
                        l = mid + side;
                        r = mid - side;
                        break;
                    }
                    case StereoPlacement::Side:
                    {
                        float mid = 0.5f * (l + r);
                        float side = 0.5f * (l - r);
                        apply (side, casR[s]);
                        l = mid + side;
                        r = mid - side;
                        break;
                    }
                    case StereoPlacement::NumPlacements:
                        break;
                }
            }
        }

        if (listening)
        {
            left[i] = listenSample;
            right[i] = listenSample;
        }
        else
        {
            left[i] = l;
            right[i] = r;
        }
    }
}

void EqEngine::process (float* left, float* right, const float* sideL, const float* sideR, int numSamples)
{
    if (left == nullptr || numSamples <= 0)
        return;
    if (right == nullptr)
        right = left;

    if (numSamples > maxBlock)
    {
        int offset = 0;
        while (offset < numSamples)
        {
            const int chunk = std::min (maxBlock, numSamples - offset);
            const float* scL = sideL != nullptr ? sideL + offset : nullptr;
            const float* scR = sideR != nullptr ? sideR + offset : nullptr;
            process (left + offset, right + offset, scL, scR, chunk);
            offset += chunk;
        }
        return;
    }

    spectrum.push (left, right, numSamples, true);

    if (currentGlobal.mode == ProcessingMode::LinearPhase)
    {
        if (numIir > 0)
        {
            const uint64_t h = bandHash();
            if (h != lastLpHash)
            {
                linear.updateFromBands (currentBands, currentGlobal.gainScale, currentGlobal.soloBand, false);
                lastLpHash = h;
            }
            linear.process (left, right, numSamples);
        }
    }
    else if (numIir > 0)
    {
        oversampler.upsample (left, right, numSamples);
        const float* scL2 = nullptr;
        const float* scR2 = nullptr;
        if (sideL != nullptr)
        {
            const int n2 = numSamples * 2;
            if (n2 <= static_cast<int> (scUpL.size()))
            {
                for (int s = 0; s < numSamples; ++s)
                {
                    scUpL[static_cast<size_t> (s * 2)]     = sideL[s];
                    scUpL[static_cast<size_t> (s * 2 + 1)] = sideL[s];
                    const float srS = sideR != nullptr ? sideR[s] : sideL[s];
                    scUpR[static_cast<size_t> (s * 2)]     = srS;
                    scUpR[static_cast<size_t> (s * 2 + 1)] = srS;
                }
                scL2 = scUpL.data();
                scR2 = scUpR.data();
            }
        }
        processIir (oversampler.left2(), oversampler.right2(), scL2, scR2, numSamples * 2);
        oversampler.downsample (left, right, numSamples);
    }

    if (anySpectral && spectral.hasWork (currentBands))
        spectral.process (left, right, numSamples, currentBands, currentGlobal.gainScale);

    // Intelligent solo: isolate the soloed band's frequency region
    if (currentGlobal.soloBand >= 0 && currentGlobal.soloBand < kMaxBands
        && ! scListen.load())
    {
        const auto& b = currentBands[static_cast<size_t> (currentGlobal.soloBand)];
        if (lastSolo != currentGlobal.soloBand || std::abs (lastDesign[static_cast<size_t> (currentGlobal.soloBand)].frequencyHz - b.frequencyHz) > 1.0f)
        {
            designBand (FilterShape::BandPass, b.frequencyHz, 0.0f, std::max (0.35f, b.q * 0.65f),
                        12.0f, false, sr, false, soloIso[0]);
            soloIso[1] = soloIso[0];
            soloIso[1].reset();
            lastSolo = currentGlobal.soloBand;
        }
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = soloIso[0].process (left[i]);
            right[i] = soloIso[1].process (right[i]);
        }
    }

    if (currentGlobal.character != CharacterMode::Off)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = applyCharacter (left[i], currentGlobal.character);
            right[i] = applyCharacter (right[i], currentGlobal.character);
        }
    }

    if (currentGlobal.autoGain && (numIir > 0 || anySpectral))
    {
        float acc = 0.0f, wacc = 0.0f;
        for (int k = 0; k < 12; ++k)
        {
            const float t = (static_cast<float> (k) + 0.5f) / 12.0f;
            const float hz = 20.0f * std::pow (1000.0f, t);
            const float mag = dbToGain (compositeMagnitudeDb (hz));
            const float w = 1.0f / std::sqrt (hz);
            acc += mag * w;
            wacc += w;
        }
        const float target = (wacc > 0.0f && acc > 1.0e-8f) ? (wacc / acc) : 1.0f;
        autoGainSmooth += 0.02f * (target - autoGainSmooth);
    }
    else
    {
        autoGainSmooth += 0.02f * (1.0f - autoGainSmooth);
    }
    autoGainDb = 20.0f * std::log10 (std::max (autoGainSmooth, 1.0e-6f));

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
    spectrum.analyzeOneHop (true);
    spectrum.analyzeOneHop (false);
}

} // namespace puzzleq

#include "dsp/EqEngine.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

void EqEngine::prepare (float sampleRate, int)
{
    sr = sampleRate;
    for (int i = 0; i < kMaxBands; ++i)
    {
        const size_t s = static_cast<size_t> (i);
        casL[s].clear();
        casR[s].clear();
        scBandL[s].clear();
        scBandR[s].clear();
        dyn[s].prepare (sampleRate);
        tptL[s].reset();
        tptR[s].reset();
        tptTiltL[s].reset();
        tptTiltR[s].reset();
        smFreq[s].setTimeMs (8.0f, sampleRate);
        smGain[s].setTimeMs (4.0f, sampleRate);
        smQ[s].setTimeMs (8.0f, sampleRate);
        smFreq[s].current = smFreq[s].target = 1000.0f;
        smGain[s].current = smGain[s].target = 0.0f;
        smQ[s].current = smQ[s].target = 1.0f;
        lastDesignedGain[s] = 1.0e9f;
        lastDesign[s] = {};
        dynGainDb[s] = 0.0f;
    }
    soloIso[0].clear();
    soloIso[1].clear();
    linear.prepare (sampleRate, currentGlobal.lpResolution);
    spectral.prepare (sampleRate);
    spectrum.prepare (sampleRate, 4096);
    autoGainSmooth = 1.0f;
    autoGainDb = 0.0f;
    lastSolo = -99;
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
        designBand (b.shape, b.frequencyHz, b.effectiveGain (currentGlobal.gainScale) + dynGainDb[static_cast<size_t> (i)],
                    b.q, b.slopeDbOct, b.brickwall, sr, natural, c);
        h *= cascadeResponse (c, hz, sr);
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
    designBand (b.shape, b.frequencyHz, b.effectiveGain (currentGlobal.gainScale) + dynGainDb[static_cast<size_t> (band)],
                b.q, b.slopeDbOct, b.brickwall, sr,
                currentGlobal.mode == ProcessingMode::NaturalPhase, c);
    return static_cast<float> (cascadeMagnitudeDb (c, hz, sr));
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
    for (int b = 0; b < kMaxBands; ++b)
    {
        const size_t s = static_cast<size_t> (b);
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

        for (int b = 0; b < kMaxBands; ++b)
        {
            const size_t s = static_cast<size_t> (b);
            const auto& band = currentBands[s];
            if (! band.isProcessing() || band.spectral)
                continue;
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
                svf.set (f, qv, sr);
                switch (band.shape)
                {
                    case FilterShape::Bell:      return svf.tickBell (x, g);
                    case FilterShape::LowShelf:  return svf.tickLowShelf (x, g);
                    case FilterShape::HighShelf: return svf.tickHighShelf (x, g);
                    case FilterShape::TiltShelf:
                        svf.set (f, 0.707f, sr);
                        tilt.set (f, 0.707f, sr);
                        return tilt.tickHighShelf (svf.tickLowShelf (x, g * 0.5f), -g * 0.5f);
                    case FilterShape::FlatTilt:
                        svf.set (650.0f, 0.5f, sr);
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
    spectrum.push (left, right, numSamples, true);

    if (currentGlobal.mode == ProcessingMode::LinearPhase)
    {
        if (--samplesUntilLinearRebuild <= 0)
        {
            linear.updateFromBands (currentBands, currentGlobal.gainScale, currentGlobal.soloBand, false);
            samplesUntilLinearRebuild = static_cast<int> (sr * 0.03f);
        }
        linear.process (left, right, numSamples);
    }
    else
    {
        processIir (left, right, sideL, sideR, numSamples);
    }

    if (spectral.hasWork (currentBands))
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

    if (currentGlobal.autoGain)
    {
        float acc = 0.0f, wacc = 0.0f;
        for (int k = 0; k < 24; ++k)
        {
            const float t = (static_cast<float> (k) + 0.5f) / 24.0f;
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
}

} // namespace puzzleq

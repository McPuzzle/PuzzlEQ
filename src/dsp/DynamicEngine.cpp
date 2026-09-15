#include "dsp/DynamicEngine.h"
#include <cmath>
#include <algorithm>

namespace puzzleq {

void EnvelopeFollower::prepare (float sampleRate) noexcept
{
    sr = sampleRate;
    reset();
}

void EnvelopeFollower::setTimes (float attackMs, float releaseMs) noexcept
{
    const float atkSec = std::max (0.0001f, attackMs * 0.001f);
    const float relSec = std::max (0.0005f, releaseMs * 0.001f);
    atk = std::exp (-1.0f / (atkSec * sr));
    rel = std::exp (-1.0f / (relSec * sr));
}

float EnvelopeFollower::processPeak (float x) noexcept
{
    const float a = std::abs (x);
    const float coeff = a > env ? atk : rel;
    env = a + coeff * (env - a);
    return env;
}

void DynamicBand::prepare (float sampleRate)
{
    sr = sampleRate;
    env.prepare (sampleRate);
    reset();
}

void DynamicBand::reset()
{
    env.reset();
    scHp.reset();
    scLp.reset();
    lastScLow = lastScHigh = -1.0f;
}

float DynamicBand::process (float sidechainSample, const BandState& band) noexcept
{
    if (std::abs (band.dynRangeDb) < 0.01f)
        return 0.0f;

    env.setTimes (band.attackMs, band.releaseMs);

    float sc = sidechainSample;
    if (band.trigger == DynamicTrigger::Free)
    {
        if (std::abs (band.scLowHz - lastScLow) > 0.5f
            || std::abs (band.scHighHz - lastScHigh) > 0.5f)
        {
            designBand (FilterShape::LowCut, band.scLowHz, 0.0f, 0.707f, 12.0f, false, sr, false, scHp);
            designBand (FilterShape::HighCut, band.scHighHz, 0.0f, 0.707f, 12.0f, false, sr, false, scLp);
            lastScLow = band.scLowHz;
            lastScHigh = band.scHighHz;
        }
        sc = scLp.process (scHp.process (sc));
    }

    const float level = env.processPeak (sc);
    const float levelDb = level > 1.0e-8f ? 20.0f * std::log10 (level) : -160.0f;
    const float over = levelDb - band.thresholdDb;

    // 0..1 amount of "how far over threshold", soft knee ~6 dB
    float amount = 0.0f;
    if (over <= -6.0f)
        amount = 0.0f;
    else if (over >= 6.0f)
        amount = 1.0f;
    else
        amount = 0.5f + over / 12.0f;

    return band.dynRangeDb * amount;
}

} // namespace puzzleq

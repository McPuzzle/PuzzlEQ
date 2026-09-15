#include "state/Presets.h"
#include "state/ParameterLayout.h"

namespace puzzleq {

static BandState makeBand (FilterShape shape, float hz, float gain, float q, float slope = 12.0f)
{
    BandState b;
    b.active = true;
    b.enabled = true;
    b.shape = shape;
    b.frequencyHz = hz;
    b.gainDb = gain;
    b.q = q;
    b.slopeDbOct = slope;
    return b;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", {} },
        { "Vocal Presence", {
            makeBand (FilterShape::LowCut, 80.0f, 0.0f, 0.7f, 12.0f),
            makeBand (FilterShape::Bell, 220.0f, -2.0f, 0.85f),
            makeBand (FilterShape::Bell, 3200.0f, 2.5f, 1.2f),
            makeBand (FilterShape::HighShelf, 10000.0f, 2.0f, 0.7f, 12.0f)
        } },
        { "Mix Bus", {
            makeBand (FilterShape::LowCut, 30.0f, 0.0f, 0.7f, 12.0f),
            makeBand (FilterShape::LowShelf, 80.0f, 0.6f, 0.7f, 12.0f),
            makeBand (FilterShape::Bell, 400.0f, -1.0f, 0.7f),
            makeBand (FilterShape::HighShelf, 10000.0f, 1.0f, 0.7f, 12.0f)
        } },
        { "Master Glue", {
            makeBand (FilterShape::LowCut, 22.0f, 0.0f, 0.7f, 12.0f),
            makeBand (FilterShape::TiltShelf, 800.0f, 1.0f, 0.7f, 12.0f),
            makeBand (FilterShape::HighShelf, 12000.0f, 0.8f, 0.7f, 6.0f)
        } },
        { "De-Mud", {
            makeBand (FilterShape::Bell, 280.0f, -3.2f, 1.4f),
            makeBand (FilterShape::Bell, 520.0f, -1.6f, 2.0f)
        } },
        { "Air", {
            makeBand (FilterShape::HighShelf, 12000.0f, 3.0f, 0.7f, 6.0f)
        } },
        { "Telephone", {
            makeBand (FilterShape::LowCut, 400.0f, 0.0f, 0.7f, 24.0f),
            makeBand (FilterShape::HighCut, 3000.0f, 0.0f, 0.7f, 24.0f)
        } },
        { "Kick Punch", {
            makeBand (FilterShape::LowShelf, 60.0f, 2.2f, 0.7f, 12.0f),
            makeBand (FilterShape::Bell, 200.0f, -3.0f, 1.5f),
            makeBand (FilterShape::Bell, 4000.0f, 2.0f, 1.1f)
        } },
        { "Snare Crack", {
            makeBand (FilterShape::Bell, 200.0f, -2.0f, 1.0f),
            makeBand (FilterShape::Bell, 5200.0f, 3.0f, 1.2f)
        } },
        { "Guitar Scoop", {
            makeBand (FilterShape::Bell, 400.0f, -3.0f, 0.8f),
            makeBand (FilterShape::HighShelf, 6000.0f, 2.0f, 0.7f, 12.0f)
        } }
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    for (int i = 0; i < kMaxBands; ++i)
        clearBand (apvts, i);

    const auto& p = presets[static_cast<size_t> (index)];
    for (size_t i = 0; i < p.bands.size() && i < static_cast<size_t> (kMaxBands); ++i)
        writeBand (apvts, static_cast<int> (i), p.bands[i]);
}

} // namespace puzzleq

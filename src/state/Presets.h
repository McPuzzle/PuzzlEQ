#pragma once

#include "state/BandState.h"
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace puzzleq {

struct FactoryPreset
{
    const char* name;
    std::vector<BandState> bands;
};

const std::vector<FactoryPreset>& factoryPresets();
void applyPreset (juce::AudioProcessorValueTreeState& apvts, int index);

} // namespace puzzleq

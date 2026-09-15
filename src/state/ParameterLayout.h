#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "state/BandState.h"
#include "state/ParameterIds.h"

namespace puzzleq {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

BandState readBand (const juce::AudioProcessorValueTreeState& apvts, int index);
GlobalState readGlobal (const juce::AudioProcessorValueTreeState& apvts);

void writeBand (juce::AudioProcessorValueTreeState& apvts, int index, const BandState& band);
void clearBand (juce::AudioProcessorValueTreeState& apvts, int index);
int findFreeBand (const juce::AudioProcessorValueTreeState& apvts);
int countActiveBands (const juce::AudioProcessorValueTreeState& apvts);

} // namespace puzzleq

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class BandInspector : public juce::Component,
                      private juce::Timer
{
public:
    explicit BandInspector (PuzzlEqAudioProcessor& proc);
    void resized() override;
    void setBand (int index);
    int currentBand() const noexcept { return band; }

private:
    void timerCallback() override;
    void rebuildAttachments();

    PuzzlEqAudioProcessor& processor;
    int band = -1;

    juce::Label title;
    juce::ComboBox shape, placement, trigger;
    juce::Slider freq, gain, q, slope, dyn, thresh, attack, release;
    juce::ToggleButton enabled, brickwall, spectral;
    juce::Label freqL, gainL, qL, slopeL, dynL, thrL, atkL, relL;

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAtt> shapeAtt, placeAtt, trigAtt;
    std::unique_ptr<SliderAtt> freqAtt, gainAtt, qAtt, slopeAtt, dynAtt, thrAtt, atkAtt, relAtt;
    std::unique_ptr<ButtonAtt> enAtt, brkAtt, spcAtt;
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PuzzlLookAndFeel.h"
#include "ui/EqDisplay.h"
#include "ui/BandInspector.h"
#include "ui/BottomBar.h"

class PuzzlEqAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PuzzlEqAudioProcessorEditor (PuzzlEqAudioProcessor&);
    ~PuzzlEqAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PuzzlLookAndFeel lnf;
    juce::Label brand;
    EqDisplay display;
    BandInspector inspector;
    BottomBar bottom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuzzlEqAudioProcessorEditor)
};

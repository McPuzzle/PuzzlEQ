#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PuzzlLookAndFeel.h"
#include "ui/EqDisplay.h"
#include "ui/BandInspector.h"
#include "ui/BottomBar.h"
#include "ui/AssistantChat.h"

class PuzzlEqAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PuzzlEqAudioProcessorEditor (PuzzlEqAudioProcessor&);
    ~PuzzlEqAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    PuzzlLookAndFeel lnf;
    juce::Label brand;
    juce::Label help;
    EqDisplay display;
    BandInspector inspector;
    BottomBar bottom;
    AssistantChat chat;
    bool fullscreen = false;
    bool helpVisible = false;
    bool chatVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuzzlEqAudioProcessorEditor)
};

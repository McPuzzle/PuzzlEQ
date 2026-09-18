#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class BottomBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit BottomBar (PuzzlEqAudioProcessor& proc);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onToggleFullscreen;
    std::function<void()> onToggleHelp;
    std::function<void()> onToggleChat;
    void setChatOpen (bool on) { btnChat.setToggleState (on, juce::dontSendNotification); }

private:
    void timerCallback() override;

    PuzzlEqAudioProcessor& processor;

    juce::ComboBox mode, resolution, character, displayRange, analyzerRange;
    juce::Slider output, gainScale, tilt, speed;
    juce::ToggleButton autoGain, bypass, phaseInvert, piano, freeze, analyzerPre;
    juce::ComboBox presetBox, instanceBox;
    juce::TextButton btnA { "A" }, btnB { "B" }, btnCopy { "Copy" }, btnPaste { "Paste" };
    juce::TextButton btnMatchSrc { "Cap Src" }, btnMatchRef { "Cap Ref" }, btnMatch { "Match" };
    juce::TextButton btnMatchOv { "Match Ov" }, btnEditRemote { "Edit Inst" };
    juce::TextButton btnLearn { "MIDI Learn" }, btnUndo { "Undo" }, btnRedo { "Redo" };
    juce::TextButton btnFull { "Full" }, btnHelp { "?" }, btnListen { "SC Listen" };
    juce::TextButton btnChat { "Chat" };
    juce::Label instanceLabel, latencyLabel;

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboAtt> modeAtt, resAtt, charAtt, dispAtt, anRangeAtt;
    std::unique_ptr<SliderAtt> outAtt, scaleAtt, tiltAtt, speedAtt;
    std::unique_ptr<ButtonAtt> autoAtt, bypassAtt, phaseAtt, pianoAtt, freezeAtt, preAtt;
};

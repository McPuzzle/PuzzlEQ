#include "PluginEditor.h"

PuzzlEqAudioProcessorEditor::PuzzlEqAudioProcessorEditor (PuzzlEqAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      display (p),
      inspector (p),
      bottom (p)
{
    setLookAndFeel (&lnf);
    brand.setText ("PuzzlEQ", juce::dontSendNotification);
    brand.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    brand.setColour (juce::Label::textColourId, lnf.curve);
    addAndMakeVisible (brand);
    addAndMakeVisible (display);
    addAndMakeVisible (inspector);
    addAndMakeVisible (bottom);

    help.setText ("PuzzlEQ  ·  Double-click add   Drag move   Multi-select Cmd-click   Wheel Q/slope   Shift-drag sketch   Cmd-click Spectrum Grab   Alt-click solo   F fullscreen   ? help\n"
                  "Edit Inst writes to the overlay instance. Match Ov fits this spectrum to the overlay. Cap Src / Cap Ref / Match for residual EQ Match. Dynamic + Spectral on the handle menu.",
                  juce::dontSendNotification);
    help.setJustificationType (juce::Justification::centredLeft);
    help.setColour (juce::Label::backgroundColourId, juce::Colour (0xee0c0e13));
    help.setColour (juce::Label::textColourId, lnf.text);
    help.setVisible (false);
    addAndMakeVisible (help);

    display.onSelectionChanged = [this]
    {
        inspector.setBand (display.selectedBand());
    };
    bottom.onToggleFullscreen = [this]
    {
        fullscreen = ! fullscreen;
        inspector.setVisible (! fullscreen);
        bottom.setVisible (true);
        resized();
    };
    bottom.onToggleHelp = [this]
    {
        helpVisible = ! helpVisible;
        help.setVisible (helpVisible);
    };

    setResizable (true, true);
    setResizeLimits (820, 520, 2200, 1400);
    setSize (1120, 720);
}

PuzzlEqAudioProcessorEditor::~PuzzlEqAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void PuzzlEqAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (lnf.bg);
    g.setColour (lnf.muted);
    g.setFont (11.0f);
    auto& p = static_cast<PuzzlEqAudioProcessor&> (processor);
    g.drawText ("v" PUZZLEQ_VERSION "  ·  " + juce::String (puzzleq::countActiveBands (p.apvts)) + " bands",
                getLocalBounds().removeFromTop (28).removeFromRight (220).reduced (8, 0),
                juce::Justification::centredRight);
}

void PuzzlEqAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto header = r.removeFromTop (28);
    brand.setBounds (header.removeFromLeft (160));
    r.removeFromTop (6);
    auto footer = r.removeFromBottom (fullscreen ? 118 : 118);
    r.removeFromBottom (8);
    if (! fullscreen)
    {
        auto inspect = r.removeFromBottom (150);
        r.removeFromBottom (8);
        inspector.setBounds (inspect);
    }
    display.setBounds (r);
    bottom.setBounds (footer);
    help.setBounds (getLocalBounds().removeFromBottom (52).reduced (12, 4));
}

bool PuzzlEqAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        fullscreen = ! fullscreen;
        inspector.setVisible (! fullscreen);
        resized();
        return true;
    }
    if (key.getTextCharacter() == '?')
    {
        helpVisible = ! helpVisible;
        help.setVisible (helpVisible);
        return true;
    }
    return display.keyPressed (key);
}

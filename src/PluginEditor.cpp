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

    display.onSelectionChanged = [this]
    {
        inspector.setBand (display.selectedBand());
    };

    setResizable (true, true);
    setResizeLimits (820, 520, 1800, 1100);
    setSize (1040, 680);
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
    g.drawText ("v" PUZZLEQ_VERSION "  ·  24-band premium EQ",
                getLocalBounds().removeFromTop (28).removeFromRight (220).reduced (8, 0),
                juce::Justification::centredRight);
}

void PuzzlEqAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto header = r.removeFromTop (28);
    brand.setBounds (header.removeFromLeft (160));
    r.removeFromTop (6);
    auto footer = r.removeFromBottom (118);
    r.removeFromBottom (8);
    auto inspect = r.removeFromBottom (150);
    r.removeFromBottom (8);
    display.setBounds (r);
    inspector.setBounds (inspect);
    bottom.setBounds (footer);
}

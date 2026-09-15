#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PuzzlLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PuzzlLookAndFeel();

    juce::Colour bg { 0xff0c0e13 };
    juce::Colour panel { 0xff141821 };
    juce::Colour panelAlt { 0xff1b2130 };
    juce::Colour grid { 0xff2a3142 };
    juce::Colour text { 0xffe7ecf4 };
    juce::Colour muted { 0xff8b95a8 };
    juce::Colour accent { 0xff7c5cff };
    juce::Colour curve { 0xff4de3c1 };
    juce::Colour spectrum { 0xaa3d7fd4 };
    juce::Colour spectrumPre { 0x5538c9a7 };
    juce::Colour handle { 0xffffc36b };
    juce::Colour danger { 0xffff6b7a };

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

#include "ui/BottomBar.h"
#include "ui/PuzzlLookAndFeel.h"

BottomBar::BottomBar (PuzzlEqAudioProcessor& proc)
    : processor (proc)
{
    mode.addItemList (juce::StringArray { "Zero Latency", "Natural Phase", "Linear Phase" }, 1);
    resolution.addItemList (juce::StringArray { "Low", "Medium", "High", "Very High", "Maximum" }, 1);
    character.addItemList (juce::StringArray { "Off", "Gentle", "Warm" }, 1);
    displayRange.addItemList (juce::StringArray { "±3 dB", "±6 dB", "±12 dB", "±30 dB" }, 1);
    analyzerRange.addItemList (juce::StringArray { "3 dB", "6 dB", "12 dB", "24 dB", "48 dB" }, 1);

    output.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    output.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    gainScale.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainScale.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    tilt.setSliderStyle (juce::Slider::LinearHorizontal);
    tilt.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    speed.setSliderStyle (juce::Slider::LinearHorizontal);
    speed.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    autoGain.setButtonText ("Auto Gain");
    phaseInvert.setButtonText ("Ø");
    piano.setButtonText ("Piano");
    freeze.setButtonText ("Freeze");
    analyzerPre.setButtonText ("Pre");

    auto& a = processor.apvts;
    modeAtt     = std::make_unique<ComboAtt>  (a, puzzleq::pid::processingMode, mode);
    resAtt      = std::make_unique<ComboAtt>  (a, puzzleq::pid::lpResolution, resolution);
    charAtt     = std::make_unique<ComboAtt>  (a, puzzleq::pid::character, character);
    dispAtt     = std::make_unique<ComboAtt>  (a, puzzleq::pid::displayRange, displayRange);
    anRangeAtt  = std::make_unique<ComboAtt>  (a, puzzleq::pid::analyzerRange, analyzerRange);
    outAtt      = std::make_unique<SliderAtt> (a, puzzleq::pid::outputGain, output);
    scaleAtt    = std::make_unique<SliderAtt> (a, puzzleq::pid::gainScale, gainScale);
    tiltAtt     = std::make_unique<SliderAtt> (a, puzzleq::pid::analyzerTilt, tilt);
    speedAtt    = std::make_unique<SliderAtt> (a, puzzleq::pid::analyzerSpeed, speed);
    autoAtt     = std::make_unique<ButtonAtt> (a, puzzleq::pid::autoGain, autoGain);
    phaseAtt    = std::make_unique<ButtonAtt> (a, puzzleq::pid::phaseInvert, phaseInvert);
    pianoAtt    = std::make_unique<ButtonAtt> (a, puzzleq::pid::pianoRoll, piano);
    freezeAtt   = std::make_unique<ButtonAtt> (a, puzzleq::pid::analyzerFreeze, freeze);
    preAtt      = std::make_unique<ButtonAtt> (a, puzzleq::pid::analyzerPre, analyzerPre);

    btnA.setClickingTogglesState (true);
    btnB.setClickingTogglesState (true);
    btnA.onClick = [this]
    {
        processor.snapshotToA();
        btnA.setToggleState (true, juce::dontSendNotification);
        btnB.setToggleState (false, juce::dontSendNotification);
    };
    btnB.onClick = [this]
    {
        processor.toggleAB();
        btnA.setToggleState (processor.isShowingA(), juce::dontSendNotification);
        btnB.setToggleState (! processor.isShowingA(), juce::dontSendNotification);
    };
    btnCopy.onClick  = [this] { processor.copyActiveBands(); };
    btnPaste.onClick = [this] { processor.pasteBands(); };
    btnUndo.onClick  = [this] { processor.undo.undo(); };
    btnRedo.onClick  = [this] { processor.undo.redo(); };

    btnMatchSrc.onClick = [this]
    {
        std::vector<float> mag;
        if (processor.engine.analyzer().consume (mag, false))
            processor.engine.matcher().accumulate (mag, static_cast<float> (processor.getSampleRate()), 4096);
        processor.engine.matcher().freezeAsSource();
    };
    btnMatchRef.onClick = [this]
    {
        std::vector<float> mag;
        if (processor.engine.analyzer().consume (mag, false))
            processor.engine.matcher().accumulate (mag, static_cast<float> (processor.getSampleRate()), 4096);
        processor.engine.matcher().freezeAsReference();
    };
    btnMatch.onClick = [this]
    {
        auto bands = processor.uiBands;
        const int n = processor.engine.matcher().fitBands (bands, 8);
        for (int i = 0; i < puzzleq::kMaxBands; ++i)
            if (bands[static_cast<size_t> (i)].active)
                puzzleq::writeBand (processor.apvts, i, bands[static_cast<size_t> (i)]);
        (void) n;
    };

    btnLearn.onClick = [this]
    {
        if (processor.isLearning())
            processor.cancelMidiLearn();
        else
            processor.beginMidiLearn (puzzleq::pid::outputGain);
        btnLearn.setToggleState (processor.isLearning(), juce::dontSendNotification);
    };
    btnLearn.setClickingTogglesState (true);

    for (auto* c : std::initializer_list<juce::Component*> {
             &mode, &resolution, &character, &displayRange, &analyzerRange,
             &output, &gainScale, &tilt, &speed,
             &autoGain, &phaseInvert, &piano, &freeze, &analyzerPre,
             &btnA, &btnB, &btnCopy, &btnPaste, &btnMatchSrc, &btnMatchRef, &btnMatch,
             &btnLearn, &btnUndo, &btnRedo, &instanceLabel })
        addAndMakeVisible (c);

    instanceLabel.setJustificationType (juce::Justification::centredLeft);
    startTimerHz (8);
}

void BottomBar::timerCallback()
{
    auto inst = PuzzlEqAudioProcessor::allInstances();
    instanceLabel.setText (processor.instanceName + "  ·  " + juce::String (static_cast<int> (inst.size()))
                               + " instance" + (inst.size() == 1 ? "" : "s"),
                           juce::dontSendNotification);
    resolution.setEnabled (mode.getSelectedItemIndex() == 2);
}

void BottomBar::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    g.setColour (lf ? lf->panel : juce::Colour (0xff141821));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // Output meter
    const float pk = std::max (processor.outputPeakL, processor.outputPeakR);
    const float db = pk > 1.0e-6f ? 20.0f * std::log10 (pk) : -60.0f;
    const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto meter = juce::Rectangle<float> (static_cast<float> (getWidth() - 88), 8.0f, 76.0f, 10.0f);
    g.setColour (lf ? lf->grid : juce::Colours::darkgrey);
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (pk > 0.99f ? juce::Colour (0xffff6b7a) : (lf ? lf->curve : juce::Colours::cyan));
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * t), 3.0f);
}

void BottomBar::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (26);
    instanceLabel.setBounds (top.removeFromLeft (180));
    btnA.setBounds (top.removeFromLeft (28));
    btnB.setBounds (top.removeFromLeft (28));
    top.removeFromLeft (4);
    btnUndo.setBounds (top.removeFromLeft (48));
    btnRedo.setBounds (top.removeFromLeft (48));
    btnCopy.setBounds (top.removeFromLeft (48));
    btnPaste.setBounds (top.removeFromLeft (52));
    top.removeFromLeft (6);
    btnMatchSrc.setBounds (top.removeFromLeft (64));
    btnMatchRef.setBounds (top.removeFromLeft (64));
    btnMatch.setBounds (top.removeFromLeft (56));
    top.removeFromLeft (6);
    btnLearn.setBounds (top.removeFromLeft (88));

    r.removeFromTop (4);
    auto mid = r.removeFromTop (22);
    mode.setBounds (mid.removeFromLeft (130));
    mid.removeFromLeft (4);
    resolution.setBounds (mid.removeFromLeft (100));
    mid.removeFromLeft (4);
    character.setBounds (mid.removeFromLeft (80));
    mid.removeFromLeft (4);
    displayRange.setBounds (mid.removeFromLeft (80));
    mid.removeFromLeft (6);
    autoGain.setBounds (mid.removeFromLeft (84));
    phaseInvert.setBounds (mid.removeFromLeft (32));
    piano.setBounds (mid.removeFromLeft (56));
    freeze.setBounds (mid.removeFromLeft (60));
    analyzerPre.setBounds (mid.removeFromLeft (44));

    r.removeFromTop (4);
    auto bot = r;
    output.setBounds (bot.removeFromLeft (64));
    gainScale.setBounds (bot.removeFromLeft (64));
    bot.removeFromLeft (8);
    analyzerRange.setBounds (bot.removeFromLeft (80).removeFromTop (22));
    tilt.setBounds (bot.removeFromLeft (120).removeFromTop (22));
    speed.setBounds (bot.removeFromLeft (120).removeFromTop (22));
}

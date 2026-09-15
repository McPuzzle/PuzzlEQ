#include "ui/BottomBar.h"
#include "ui/PuzzlLookAndFeel.h"
#include "state/Presets.h"
#include <algorithm>

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

    auto snapSr = [this]
    {
        return processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    };
    auto snapN = [this] { return std::max (256, processor.engine.analyzer().fftSize()); };

    btnMatchSrc.onClick = [this, snapSr, snapN]
    {
        std::vector<float> mag;
        if (processor.engine.analyzer().copyCurrent (mag, true)
            || processor.engine.analyzer().copyCurrent (mag, false))
            processor.engine.matcher().setSourceFrom (mag, snapSr(), snapN());
    };
    btnMatchRef.onClick = [this, snapSr, snapN]
    {
        std::vector<float> mag;
        if (processor.overlayInstance != nullptr && processor.overlayInstance != &processor)
            processor.overlayInstance->copyPublishedSpectrum (mag);
        if (mag.size() < 8)
            processor.engine.analyzer().copyCurrent (mag, false);
        if (mag.size() >= 8)
            processor.engine.matcher().setReferenceFrom (mag, snapSr(), snapN());
    };
    auto applyMatch = [this]
    {
        auto& target = processor.editTarget();
        auto bands = target.uiBands;
        const int n = processor.engine.matcher().fitBands (bands, 10);
        for (int i = 0; i < puzzleq::kMaxBands; ++i)
            if (bands[static_cast<size_t> (i)].active)
                puzzleq::writeBand (target.apvts, i, bands[static_cast<size_t> (i)]);
        target.pullStateFromApvts();
        (void) n;
    };
    btnMatch.onClick = applyMatch;
    btnMatchOv.onClick = [this, snapSr, snapN, applyMatch]
    {
        std::vector<float> src, ref;
        processor.engine.analyzer().copyCurrent (src, true);
        if (src.size() < 8)
            processor.engine.analyzer().copyCurrent (src, false);
        if (processor.overlayInstance != nullptr)
            processor.overlayInstance->copyPublishedSpectrum (ref);
        if (src.size() < 8 || ref.size() < 8)
            return;
        processor.engine.matcher().setSourceFrom (src, snapSr(), snapN());
        processor.engine.matcher().setReferenceFrom (ref, snapSr(), snapN());
        applyMatch();
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

    const auto& presets = puzzleq::factoryPresets();
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
        presetBox.addItem (presets[static_cast<size_t> (i)].name, i + 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.applyFactoryPreset (presetBox.getSelectedId() - 1);
    };

    instanceBox.onChange = [this]
    {
        auto inst = PuzzlEqAudioProcessor::allInstances();
        const int idx = instanceBox.getSelectedId() - 1;
        if (idx >= 0 && idx < static_cast<int> (inst.size()))
        {
            processor.overlayInstance = inst[static_cast<size_t> (idx)];
            if (inst[static_cast<size_t> (idx)] == &processor)
                processor.editRemote = false;
        }
    };

    btnEditRemote.setClickingTogglesState (true);
    btnEditRemote.onClick = [this]
    {
        const bool on = btnEditRemote.getToggleState();
        if (on && (processor.overlayInstance == nullptr || processor.overlayInstance == &processor))
        {
            btnEditRemote.setToggleState (false, juce::dontSendNotification);
            processor.editRemote = false;
            return;
        }
        processor.editRemote = on;
    };

    btnFull.setClickingTogglesState (true);
    btnFull.onClick = [this] { if (onToggleFullscreen) onToggleFullscreen(); };
    btnHelp.setClickingTogglesState (true);
    btnHelp.onClick = [this] { if (onToggleHelp) onToggleHelp(); };
    btnListen.setClickingTogglesState (true);
    btnListen.onClick = [this]
    {
        processor.sidechainListen = btnListen.getToggleState();
    };

    for (auto* c : std::initializer_list<juce::Component*> {
             &mode, &resolution, &character, &displayRange, &analyzerRange,
             &output, &gainScale, &tilt, &speed,
             &autoGain, &phaseInvert, &piano, &freeze, &analyzerPre,
             &btnA, &btnB, &btnCopy, &btnPaste, &btnMatchSrc, &btnMatchRef, &btnMatch,
             &btnMatchOv, &btnEditRemote,
             &btnLearn, &btnUndo, &btnRedo, &instanceLabel, &presetBox, &instanceBox,
             &btnFull, &btnHelp, &btnListen, &latencyLabel })
        addAndMakeVisible (c);

    instanceLabel.setJustificationType (juce::Justification::centredLeft);
    latencyLabel.setJustificationType (juce::Justification::centredRight);
    startTimerHz (8);
}

void BottomBar::timerCallback()
{
    auto inst = PuzzlEqAudioProcessor::allInstances();
    instanceLabel.setText (processor.instanceName + "  ·  " + juce::String (static_cast<int> (inst.size()))
                               + " instance" + (inst.size() == 1 ? "" : "s"),
                           juce::dontSendNotification);
    resolution.setEnabled (mode.getSelectedItemIndex() == 2);

    const int lat = processor.engine.latencySamples();
    const float ms = processor.getSampleRate() > 0
        ? 1000.0f * static_cast<float> (lat) / static_cast<float> (processor.getSampleRate())
        : 0.0f;
    latencyLabel.setText (juce::String (lat) + " smp  " + juce::String (ms, 1) + " ms"
                              + (processor.engine.lastAutoGainDb() != 0.0f
                                     ? ("  AG " + juce::String (processor.engine.lastAutoGainDb(), 1) + " dB")
                                     : juce::String()),
                          juce::dontSendNotification);

    btnEditRemote.setToggleState (processor.isEditingRemote(), juce::dontSendNotification);
    btnEditRemote.setEnabled (processor.overlayInstance != nullptr && processor.overlayInstance != &processor);

    if (instanceBox.getNumItems() != static_cast<int> (inst.size()))
    {
        instanceBox.clear (juce::dontSendNotification);
        for (int i = 0; i < static_cast<int> (inst.size()); ++i)
            instanceBox.addItem (inst[static_cast<size_t> (i)]->instanceName
                                     + (inst[static_cast<size_t> (i)] == &processor ? " (this)" : " — overlay"),
                                 i + 1);
        instanceBox.setSelectedId (1, juce::dontSendNotification);
    }
}

void BottomBar::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    g.setColour (lf ? lf->panel : juce::Colour (0xff141821));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    auto drawMeter = [&] (float pk, float x, float y, float w)
    {
        const float db = pk > 1.0e-6f ? 20.0f * std::log10 (pk) : -60.0f;
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto meter = juce::Rectangle<float> (x, y, w, 5.0f);
        g.setColour (lf ? lf->grid : juce::Colours::darkgrey);
        g.fillRoundedRectangle (meter, 2.0f);
        g.setColour (pk > 0.99f ? juce::Colour (0xffff6b7a) : (lf ? lf->curve : juce::Colours::cyan));
        g.fillRoundedRectangle (meter.withWidth (w * t), 2.0f);
    };
    const float mx = static_cast<float> (getWidth() - 100);
    drawMeter (processor.inputPeakL, mx, 6.0f, 88.0f);
    drawMeter (processor.inputPeakR, mx, 12.0f, 88.0f);
    drawMeter (processor.outputPeakL, mx, 20.0f, 88.0f);
    drawMeter (processor.outputPeakR, mx, 26.0f, 88.0f);
}

void BottomBar::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (26);
    instanceLabel.setBounds (top.removeFromLeft (150));
    presetBox.setBounds (top.removeFromLeft (118));
    top.removeFromLeft (4);
    instanceBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (4);
    btnEditRemote.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (4);
    btnA.setBounds (top.removeFromLeft (26));
    btnB.setBounds (top.removeFromLeft (26));
    top.removeFromLeft (4);
    btnUndo.setBounds (top.removeFromLeft (44));
    btnRedo.setBounds (top.removeFromLeft (44));
    btnCopy.setBounds (top.removeFromLeft (44));
    btnPaste.setBounds (top.removeFromLeft (48));
    top.removeFromLeft (4);
    btnMatchSrc.setBounds (top.removeFromLeft (60));
    btnMatchRef.setBounds (top.removeFromLeft (60));
    btnMatch.setBounds (top.removeFromLeft (48));
    btnMatchOv.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (4);
    btnLearn.setBounds (top.removeFromLeft (80));
    btnListen.setBounds (top.removeFromLeft (78));
    btnFull.setBounds (top.removeFromLeft (40));
    btnHelp.setBounds (top.removeFromLeft (28));

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
    bot.removeFromLeft (8);
    latencyLabel.setBounds (bot.removeFromTop (22));
}

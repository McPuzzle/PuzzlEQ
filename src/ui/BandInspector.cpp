#include "ui/BandInspector.h"
#include "state/Types.h"

BandInspector::BandInspector (PuzzlEqAudioProcessor& proc)
    : processor (proc)
{
    title.setText ("No band selected — click the graph or + Add Band", juce::dontSendNotification);
    addAndMakeVisible (title);
    addBandBtn.onClick = [this]
    {
        const int slot = puzzleq::findFreeBand (processor.editApvts());
        if (slot < 0)
            return;
        puzzleq::BandState b;
        b.active = true;
        b.enabled = true;
        b.shape = puzzleq::FilterShape::Bell;
        b.frequencyHz = 1000.0f;
        processor.undo.beginNewTransaction ("Add band");
        puzzleq::writeBand (processor.editApvts(), slot, b);
        processor.editTarget().uiBands[static_cast<size_t> (slot)] = b;
        processor.editTarget().engine.setBands (processor.editTarget().uiBands);
        processor.editTarget().markLocalEdit();
        processor.editTarget().commitUiBandsToHost();
        setBand (slot);
    };
    addAndMakeVisible (addBandBtn);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name,
                             juce::NormalisableRange<double> range, double start)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        s.setNormalisableRange (range);
        s.setValue (start, juce::dontSendNotification);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (s);
        addAndMakeVisible (l);
    };

    setupKnob (freq, freqL, "Freq",
               { (double) puzzleq::kMinHz, (double) puzzleq::kMaxHz, 0.01, 0.3 }, 1000.0);
    freq.setTextValueSuffix (" Hz");
    setupKnob (gain, gainL, "Gain",
               { (double) puzzleq::kMinGainDb, (double) puzzleq::kMaxGainDb, 0.01 }, 0.0);
    gain.setTextValueSuffix (" dB");
    setupKnob (q, qL, "Q",
               { (double) puzzleq::kMinQ, (double) puzzleq::kMaxQ, 0.001, 0.4 }, 1.0);
    setupKnob (slope, slopeL, "Slope",
               { (double) puzzleq::kMinSlope, (double) puzzleq::kMaxSlope, 0.5 }, 12.0);
    setupKnob (dyn, dynL, "Dyn", { -30.0, 30.0, 0.01 }, 0.0);
    setupKnob (thresh, thrL, "Thresh", { -60.0, 0.0, 0.01 }, -24.0);
    setupKnob (attack, atkL, "Attack", { 0.1, 200.0, 0.01, 0.4 }, 12.0);
    setupKnob (release, relL, "Release", { 1.0, 2000.0, 0.1, 0.4 }, 80.0);

    shape.addItemList (juce::StringArray {
        "Bell", "Notch", "High Shelf", "Low Shelf", "High Cut",
        "Low Cut", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" }, 1);
    placement.addItemList (juce::StringArray { "Stereo", "Left", "Right", "Mid", "Side" }, 1);
    trigger.addItemList (juce::StringArray { "Band", "Free", "External" }, 1);

    enabled.setButtonText ("On");
    enabled.setToggleState (true, juce::dontSendNotification);
    brickwall.setButtonText ("Brickwall");
    spectral.setButtonText ("Spectral");

    addAndMakeVisible (shape);
    addAndMakeVisible (placement);
    addAndMakeVisible (trigger);
    addAndMakeVisible (enabled);
    addAndMakeVisible (brickwall);
    addAndMakeVisible (spectral);

    startTimerHz (10);
}

void BandInspector::setBand (int index)
{
    band = index;
    rebuildAttachments();
    resized();
}

void BandInspector::rebuildAttachments()
{
    shapeAtt.reset(); placeAtt.reset(); trigAtt.reset();
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); slopeAtt.reset();
    dynAtt.reset(); thrAtt.reset(); atkAtt.reset(); relAtt.reset();
    enAtt.reset(); brkAtt.reset(); spcAtt.reset();

    auto bandIsLive = [this] (int i) -> bool
    {
        if (i < 0 || i >= puzzleq::kMaxBands)
            return false;
        if (processor.editTarget().uiBands[static_cast<size_t> (i)].active)
            return true;
        return puzzleq::readBand (processor.editApvts(), i).active;
    };

    if (! bandIsLive (band))
    {
        title.setText ("No band selected — click the graph or + Add Band", juce::dontSendNotification);
        return;
    }

    title.setText ((processor.isEditingRemote() ? ("Remote · " + processor.editTarget().instanceName + " · ") : juce::String())
                       + "Band " + juce::String (band + 1),
                   juce::dontSendNotification);
    auto& a = processor.editApvts();
    const int i = band;

    // Range is already set in the ctor. Re-assert before attaching so a default
    // 0–10 slider cannot write 0 Hz / 0 dB/oct into the parameters.
    freq.setNormalisableRange ({ (double) puzzleq::kMinHz, (double) puzzleq::kMaxHz, 0.01, 0.3 });
    gain.setNormalisableRange ({ (double) puzzleq::kMinGainDb, (double) puzzleq::kMaxGainDb, 0.01 });
    q.setNormalisableRange ({ (double) puzzleq::kMinQ, (double) puzzleq::kMaxQ, 0.001, 0.4 });
    slope.setNormalisableRange ({ (double) puzzleq::kMinSlope, (double) puzzleq::kMaxSlope, 0.5 });
    dyn.setNormalisableRange ({ -30.0, 30.0, 0.01 });
    thresh.setNormalisableRange ({ -60.0, 0.0, 0.01 });
    attack.setNormalisableRange ({ 0.1, 200.0, 0.01, 0.4 });
    release.setNormalisableRange ({ 1.0, 2000.0, 0.1, 0.4 });

    const auto st = processor.editTarget().uiBands[static_cast<size_t> (i)];
    freq.setValue (st.frequencyHz, juce::dontSendNotification);
    gain.setValue (st.gainDb, juce::dontSendNotification);
    q.setValue (st.q, juce::dontSendNotification);
    slope.setValue (st.slopeDbOct, juce::dontSendNotification);
    dyn.setValue (st.dynRangeDb, juce::dontSendNotification);
    thresh.setValue (st.thresholdDb, juce::dontSendNotification);
    attack.setValue (st.attackMs, juce::dontSendNotification);
    release.setValue (st.releaseMs, juce::dontSendNotification);
    enabled.setToggleState (st.enabled, juce::dontSendNotification);
    brickwall.setToggleState (st.brickwall, juce::dontSendNotification);
    spectral.setToggleState (st.spectral, juce::dontSendNotification);

    shapeAtt  = std::make_unique<ComboAtt>  (a, puzzleq::bandId (i, "shp"), shape);
    placeAtt  = std::make_unique<ComboAtt>  (a, puzzleq::bandId (i, "plc"), placement);
    trigAtt   = std::make_unique<ComboAtt>  (a, puzzleq::bandId (i, "trg"), trigger);
    freqAtt   = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "frq"), freq);
    gainAtt   = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "gn"), gain);
    qAtt      = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "q"), q);
    slopeAtt  = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "slp"), slope);
    dynAtt    = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "dyn"), dyn);
    thrAtt    = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "thr"), thresh);
    atkAtt    = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "atk"), attack);
    relAtt    = std::make_unique<SliderAtt> (a, puzzleq::bandId (i, "rel"), release);
    enAtt     = std::make_unique<ButtonAtt> (a, puzzleq::bandId (i, "en"), enabled);
    brkAtt    = std::make_unique<ButtonAtt> (a, puzzleq::bandId (i, "brk"), brickwall);
    spcAtt    = std::make_unique<ButtonAtt> (a, puzzleq::bandId (i, "spc"), spectral);
}

void BandInspector::timerCallback()
{
    void* cur = &processor.editApvts();
    if (cur != lastEditApvts)
    {
        lastEditApvts = cur;
        rebuildAttachments();
    }

    const bool on = band >= 0 && band < puzzleq::kMaxBands
                 && processor.editTarget().uiBands[static_cast<size_t> (band)].active;
    addBandBtn.setVisible (! on);
    for (auto* c : std::initializer_list<juce::Component*> {
             &shape, &placement, &trigger, &freq, &gain, &q, &slope,
             &dyn, &thresh, &attack, &release, &enabled, &brickwall, &spectral,
             &freqL, &gainL, &qL, &slopeL, &dynL, &thrL, &atkL, &relL })
    {
        c->setVisible (on);
        c->setEnabled (on);
    }
    if (! on && band >= 0)
        title.setText ("No band selected — click the graph or + Add Band", juce::dontSendNotification);
}

void BandInspector::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (18);
    title.setBounds (header.removeFromLeft (juce::jmax (120, header.getWidth() - 110)));
    addBandBtn.setBounds (header.removeFromRight (100));
    r.removeFromTop (4);
    auto row1 = r.removeFromTop (22);
    shape.setBounds (row1.removeFromLeft (130));
    row1.removeFromLeft (6);
    placement.setBounds (row1.removeFromLeft (90));
    row1.removeFromLeft (6);
    trigger.setBounds (row1.removeFromLeft (90));
    row1.removeFromLeft (8);
    enabled.setBounds (row1.removeFromLeft (50));
    brickwall.setBounds (row1.removeFromLeft (80));
    spectral.setBounds (row1.removeFromLeft (80));

    r.removeFromTop (6);
    auto knobs = r;
    const int n = 8;
    const int w = knobs.getWidth() / n;
    juce::Slider* sliders[] = { &freq, &gain, &q, &slope, &dyn, &thresh, &attack, &release };
    juce::Label* labels[] = { &freqL, &gainL, &qL, &slopeL, &dynL, &thrL, &atkL, &relL };
    for (int i = 0; i < n; ++i)
    {
        auto cell = knobs.removeFromLeft (w);
        labels[i]->setBounds (cell.removeFromTop (14));
        sliders[i]->setBounds (cell);
    }
}

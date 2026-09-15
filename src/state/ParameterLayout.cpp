#include "state/ParameterLayout.h"

namespace puzzleq {

namespace {
juce::String bandName (int band, const juce::String& label)
{
    return "Band " + juce::String (band + 1) + " " + label;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addBool = [&] (const juce::String& id, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, def));
    };

    auto addFloat = [&] (const juce::String& id, const juce::String& name,
                         juce::NormalisableRange<float> range, float def,
                         const juce::String& suffix = {})
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (suffix)));
    };

    auto addChoice = [&] (const juce::String& id, const juce::String& name,
                          const juce::StringArray& choices, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, choices, def));
    };

    auto addInt = [&] (const juce::String& id, const juce::String& name, int mn, int mx, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, mn, mx, def));
    };

    juce::StringArray shapes;
    for (auto* n : kShapeNames) shapes.add (n);
    juce::StringArray placements;
    for (auto* n : kPlacementNames) placements.add (n);
    juce::StringArray triggers { "Band", "Free", "External" };

    for (int i = 0; i < kMaxBands; ++i)
    {
        addBool   (bandId (i, "act"),   bandName (i, "Active"), false);
        addBool   (bandId (i, "en"),    bandName (i, "Enabled"), true);
        addChoice (bandId (i, "shp"),   bandName (i, "Shape"), shapes, 0);
        addFloat  (bandId (i, "frq"),   bandName (i, "Frequency"),
                   juce::NormalisableRange<float> (kMinHz, kMaxHz, 0.01f, 0.3f), 1000.0f, "Hz");
        addFloat  (bandId (i, "gn"),    bandName (i, "Gain"),
                   juce::NormalisableRange<float> (kMinGainDb, kMaxGainDb, 0.01f), 0.0f, "dB");
        addFloat  (bandId (i, "q"),     bandName (i, "Q"),
                   juce::NormalisableRange<float> (kMinQ, kMaxQ, 0.001f, 0.4f), 1.0f);
        addFloat  (bandId (i, "slp"),   bandName (i, "Slope"),
                   juce::NormalisableRange<float> (kMinSlope, kMaxSlope, 0.5f), 12.0f, "dB/oct");
        addBool   (bandId (i, "brk"),   bandName (i, "Brickwall"), false);
        addChoice (bandId (i, "plc"),   bandName (i, "Placement"), placements, 0);
        addFloat  (bandId (i, "dyn"),   bandName (i, "Dyn Range"),
                   juce::NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f, "dB");
        addFloat  (bandId (i, "thr"),   bandName (i, "Threshold"),
                   juce::NormalisableRange<float> (-60.0f, 0.0f, 0.01f), -24.0f, "dB");
        addFloat  (bandId (i, "atk"),   bandName (i, "Attack"),
                   juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.4f), 12.0f, "ms");
        addFloat  (bandId (i, "rel"),   bandName (i, "Release"),
                   juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 80.0f, "ms");
        addBool   (bandId (i, "spc"),   bandName (i, "Spectral"), false);
        addChoice (bandId (i, "trg"),   bandName (i, "Trigger"), triggers, 0);
        addFloat  (bandId (i, "scl"),   bandName (i, "SC Low"),
                   juce::NormalisableRange<float> (kMinHz, kMaxHz, 0.01f, 0.3f), 20.0f, "Hz");
        addFloat  (bandId (i, "sch"),   bandName (i, "SC High"),
                   juce::NormalisableRange<float> (kMinHz, kMaxHz, 0.01f, 0.3f), 20000.0f, "Hz");
    }

    addFloat (pid::outputGain, "Output Gain",
              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, "dB");
    addChoice (pid::processingMode, "Processing Mode",
               juce::StringArray { "Zero Latency", "Natural Phase", "Linear Phase" }, 0);
    addChoice (pid::lpResolution, "Linear Resolution",
               juce::StringArray { "Low", "Medium", "High", "Very High", "Maximum" }, 1);
    addBool (pid::autoGain, "Auto Gain", false);
    addFloat (pid::gainScale, "Gain Scale",
              juce::NormalisableRange<float> (0.25f, 2.0f, 0.01f), 1.0f, "x");
    addChoice (pid::character, "Character",
               juce::StringArray { "Off", "Gentle", "Warm" }, 0);
    addBool (pid::phaseInvert, "Phase Invert", false);
    addBool (pid::pianoRoll, "Piano Roll", false);
    addFloat (pid::analyzerTilt, "Analyzer Tilt",
              juce::NormalisableRange<float> (0.0f, 9.0f, 0.1f), 4.5f, "dB/oct");
    addChoice (pid::analyzerRange, "Analyzer Range",
               juce::StringArray { "3 dB", "6 dB", "12 dB", "24 dB", "48 dB" }, 2);
    addFloat (pid::analyzerSpeed, "Analyzer Speed",
              juce::NormalisableRange<float> (0.05f, 1.0f, 0.01f), 0.55f);
    addBool (pid::analyzerFreeze, "Analyzer Freeze", false);
    addBool (pid::analyzerPre, "Analyzer Pre", false);
    addInt (pid::soloBand, "Solo Band", -1, kMaxBands - 1, -1);
    addChoice (pid::displayRange, "Display Range",
               juce::StringArray { "3 dB", "6 dB", "12 dB", "30 dB" }, 2);

    return { params.begin(), params.end() };
}

static float raw (const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback)
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return fallback;
}

BandState readBand (const juce::AudioProcessorValueTreeState& apvts, int index)
{
    BandState b;
    b.active        = raw (apvts, bandId (index, "act"), 0.0f) > 0.5f;
    b.enabled       = raw (apvts, bandId (index, "en"), 1.0f) > 0.5f;
    b.shape         = static_cast<FilterShape> (juce::roundToInt (raw (apvts, bandId (index, "shp"), 0.0f)));
    b.frequencyHz   = raw (apvts, bandId (index, "frq"), 1000.0f);
    b.gainDb        = raw (apvts, bandId (index, "gn"), 0.0f);
    b.q             = raw (apvts, bandId (index, "q"), 1.0f);
    b.slopeDbOct    = raw (apvts, bandId (index, "slp"), 12.0f);
    b.brickwall     = raw (apvts, bandId (index, "brk"), 0.0f) > 0.5f;
    b.placement     = static_cast<StereoPlacement> (juce::roundToInt (raw (apvts, bandId (index, "plc"), 0.0f)));
    b.dynRangeDb    = raw (apvts, bandId (index, "dyn"), 0.0f);
    b.thresholdDb   = raw (apvts, bandId (index, "thr"), -24.0f);
    b.attackMs      = raw (apvts, bandId (index, "atk"), 12.0f);
    b.releaseMs     = raw (apvts, bandId (index, "rel"), 80.0f);
    b.spectral      = raw (apvts, bandId (index, "spc"), 0.0f) > 0.5f;
    b.trigger       = static_cast<DynamicTrigger> (juce::roundToInt (raw (apvts, bandId (index, "trg"), 0.0f)));
    b.scLowHz       = raw (apvts, bandId (index, "scl"), 20.0f);
    b.scHighHz      = raw (apvts, bandId (index, "sch"), 20000.0f);
    return b;
}

GlobalState readGlobal (const juce::AudioProcessorValueTreeState& apvts)
{
    GlobalState g;
    g.outputGainDb   = raw (apvts, pid::outputGain, 0.0f);
    g.mode           = static_cast<ProcessingMode> (juce::roundToInt (raw (apvts, pid::processingMode, 0.0f)));
    g.lpResolution   = static_cast<LinearResolution> (juce::roundToInt (raw (apvts, pid::lpResolution, 1.0f)));
    g.autoGain       = raw (apvts, pid::autoGain, 0.0f) > 0.5f;
    g.gainScale      = raw (apvts, pid::gainScale, 1.0f);
    g.character      = static_cast<CharacterMode> (juce::roundToInt (raw (apvts, pid::character, 0.0f)));
    g.phaseInvert    = raw (apvts, pid::phaseInvert, 0.0f) > 0.5f;
    g.pianoRoll      = raw (apvts, pid::pianoRoll, 0.0f) > 0.5f;
    g.analyzerTilt   = raw (apvts, pid::analyzerTilt, 4.5f);
    {
        const int r = juce::roundToInt (raw (apvts, pid::analyzerRange, 2.0f));
        constexpr float ranges[] = { 3.f, 6.f, 12.f, 24.f, 48.f };
        g.analyzerRangeDb = ranges[juce::jlimit (0, 4, r)];
    }
    g.analyzerSpeed  = raw (apvts, pid::analyzerSpeed, 0.55f);
    g.analyzerFreeze = raw (apvts, pid::analyzerFreeze, 0.0f) > 0.5f;
    g.analyzerPre    = raw (apvts, pid::analyzerPre, 0.0f) > 0.5f;
    g.soloBand       = juce::roundToInt (raw (apvts, pid::soloBand, -1.0f));
    {
        const int r = juce::roundToInt (raw (apvts, pid::displayRange, 2.0f));
        constexpr int ranges[] = { 3, 6, 12, 30 };
        g.displayRangeDb = ranges[juce::jlimit (0, 3, r)];
    }
    return g;
}

void writeBand (juce::AudioProcessorValueTreeState& apvts, int index, const BandState& band)
{
    auto setFloat = [&] (const juce::String& id, float v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id)))
        {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
        else if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    auto setBool = [&] (const juce::String& id, bool v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)))
        {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
        else if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v ? 1.0f : 0.0f);
            p->endChangeGesture();
        }
    };
    auto setChoice = [&] (const juce::String& id, int v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        {
            p->beginChangeGesture();
            *p = v;
            p->endChangeGesture();
        }
        else if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (v)));
            p->endChangeGesture();
        }
    };

    setBool (bandId (index, "act"), band.active);
    setBool (bandId (index, "en"), band.enabled);
    setChoice (bandId (index, "shp"), static_cast<int> (band.shape));
    setFloat (bandId (index, "frq"), band.frequencyHz);
    setFloat (bandId (index, "gn"), band.gainDb);
    setFloat (bandId (index, "q"), band.q);
    setFloat (bandId (index, "slp"), band.slopeDbOct);
    setBool (bandId (index, "brk"), band.brickwall);
    setChoice (bandId (index, "plc"), static_cast<int> (band.placement));
    setFloat (bandId (index, "dyn"), band.dynRangeDb);
    setFloat (bandId (index, "thr"), band.thresholdDb);
    setFloat (bandId (index, "atk"), band.attackMs);
    setFloat (bandId (index, "rel"), band.releaseMs);
    setBool (bandId (index, "spc"), band.spectral);
    setChoice (bandId (index, "trg"), static_cast<int> (band.trigger));
    setFloat (bandId (index, "scl"), band.scLowHz);
    setFloat (bandId (index, "sch"), band.scHighHz);
}

void clearBand (juce::AudioProcessorValueTreeState& apvts, int index)
{
    BandState empty;
    empty.active = false;
    writeBand (apvts, index, empty);
}

int findFreeBand (const juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < kMaxBands; ++i)
        if (raw (apvts, bandId (i, "act"), 0.0f) < 0.5f)
            return i;
    return -1;
}

int countActiveBands (const juce::AudioProcessorValueTreeState& apvts)
{
    int n = 0;
    for (int i = 0; i < kMaxBands; ++i)
        if (raw (apvts, bandId (i, "act"), 0.0f) > 0.5f)
            ++n;
    return n;
}

} // namespace puzzleq

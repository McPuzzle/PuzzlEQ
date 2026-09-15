#include "ui/EqDisplay.h"
#include "ui/PuzzlLookAndFeel.h"
#include "dsp/FilterDesign.h"
#include <algorithm>

namespace {
constexpr float kMinPlotHz = 20.0f;
constexpr float kMaxPlotHz = 20000.0f;
}

EqDisplay::EqDisplay (PuzzlEqAudioProcessor& proc)
    : processor (proc)
{
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

EqDisplay::~EqDisplay()
{
    stopTimer();
}

void EqDisplay::timerCallback()
{
    auto& target = processor.editTarget();
    target.pullStateFromApvts();
    target.engine.setBands (target.uiBands);
    target.engine.setGlobal (target.uiGlobal);
    if (&target != &processor)
        processor.pullStateFromApvts();

    if (processor.isEditingRemote())
    {
        target.copyPublishedSpectrum (postMag);
        processor.copyPublishedSpectrum (overlayMag);
        preMag.clear();
        postPeaks.clear();
        target.engine.analyzer().copyCurrent (postPeaks, false);
    }
    else
    {
        auto& an = processor.engine.analyzer();
        an.consume (preMag, true);
        an.consume (postMag, false);
        an.consumePeaks (postPeaks, false);
        if (processor.overlayInstance != nullptr && processor.overlayInstance != &processor)
            processor.overlayInstance->copyPublishedSpectrum (overlayMag);
        else
            overlayMag.clear();
    }
    processor.selectedBandForListen = primarySel;
    repaint();
}

juce::Rectangle<float> EqDisplay::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 18.0f);
}

float EqDisplay::xToHz (float x) const
{
    const auto r = plotBounds();
    const float t = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    return kMinPlotHz * std::pow (kMaxPlotHz / kMinPlotHz, t);
}

float EqDisplay::hzToX (float hz) const
{
    const auto r = plotBounds();
    const float t = std::log (juce::jlimit (kMinPlotHz, kMaxPlotHz, hz) / kMinPlotHz)
                  / std::log (kMaxPlotHz / kMinPlotHz);
    return r.getX() + t * r.getWidth();
}

float EqDisplay::yToDb (float y) const
{
    const auto r = plotBounds();
    const float range = static_cast<float> (processor.uiGlobal.displayRangeDb);
    const float t = (y - r.getY()) / r.getHeight();
    return range * (1.0f - 2.0f * t);
}

float EqDisplay::dbToY (float db) const
{
    const auto r = plotBounds();
    const float range = static_cast<float> (processor.uiGlobal.displayRangeDb);
    const float t = 0.5f - db / (2.0f * range);
    return r.getY() + juce::jlimit (0.0f, 1.0f, t) * r.getHeight();
}

juce::Point<float> EqDisplay::bandToPoint (const puzzleq::BandState& b) const
{
    const float db = puzzleq::shapeUsesGain (b.shape) ? b.gainDb : 0.0f;
    return { hzToX (b.frequencyHz), dbToY (db) };
}

int EqDisplay::hitTestBand (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 14.0f;
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        const auto& b = processor.editTarget().uiBands[static_cast<size_t> (i)];
        if (! b.active)
            continue;
        const auto q = bandToPoint (b);
        const float d = q.getDistanceFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

int EqDisplay::hitTestSpectrumPeak (juce::Point<float> p) const
{
    if (postMag.size() < 8)
        return -1;
    const int n = (static_cast<int> (postMag.size()) - 1) * 2;
    const float sr = processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    int best = -1;
    float bestD = 10.0f;
    for (int i = 2; i < static_cast<int> (postMag.size()) - 2; ++i)
    {
        const float hz = puzzleq::SpectrumAnalyzer::binToHz (i, n, sr);
        if (hz < 30.0f || hz > 16000.0f)
            continue;
        const float db = postMag[static_cast<size_t> (i)];
        // local peak
        if (db < postMag[static_cast<size_t> (i - 1)] || db < postMag[static_cast<size_t> (i + 1)])
            continue;
        const auto pt = juce::Point<float> (hzToX (hz), dbToY (juce::jlimit (-30.0f, 30.0f, db + 12.0f)));
        const float d = pt.getDistanceFrom (p);
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void EqDisplay::setSelectedBand (int b)
{
    primarySel = b;
    selection.clear();
    if (b >= 0)
        selection.push_back (b);
    if (onSelectionChanged)
        onSelectionChanged();
}

void EqDisplay::addBandAt (float hz, float db)
{
    auto& ap = processor.editApvts();
    const int slot = puzzleq::findFreeBand (ap);
    if (slot < 0)
        return;
    puzzleq::BandState b;
    b.active = true;
    b.enabled = true;
    b.shape = puzzleq::FilterShape::Bell;
    b.frequencyHz = juce::jlimit (puzzleq::kMinHz, puzzleq::kMaxHz, hz);
    b.gainDb = juce::jlimit (puzzleq::kMinGainDb, puzzleq::kMaxGainDb, db);
    b.q = 1.0f;
    puzzleq::writeBand (ap, slot, b);
    processor.editTarget().pullStateFromApvts();
    setSelectedBand (slot);
}

void EqDisplay::updateBandFromDrag (int band, juce::Point<float> p, bool quantize)
{
    auto& ap = processor.editApvts();
    auto st = puzzleq::readBand (ap, band);
    float hz = xToHz (p.x);
    if (quantize || processor.uiGlobal.pianoRoll)
        hz = puzzleq::noteToHz (puzzleq::hzToNearestNote (hz));
    st.frequencyHz = juce::jlimit (puzzleq::kMinHz, puzzleq::kMaxHz, hz);
    if (puzzleq::shapeUsesGain (st.shape))
        st.gainDb = juce::jlimit (puzzleq::kMinGainDb, puzzleq::kMaxGainDb, yToDb (p.y));
    puzzleq::writeBand (ap, band, st);
}

void EqDisplay::showValueEditor (int band, juce::Point<int> at)
{
    editorBand = band;
    editor = std::make_unique<juce::TextEditor>();
    auto& ap = processor.editApvts();
    auto st = puzzleq::readBand (ap, band);
    editor->setText (juce::String (st.frequencyHz, 1) + "  " + juce::String (st.gainDb, 1)
                     + "  " + juce::String (st.q, 2));
    editor->setBounds (at.x, at.y, 180, 22);
    editor->onReturnKey = [this]
    {
        if (editor == nullptr)
            return;
        auto toks = juce::StringArray::fromTokens (editor->getText(), " ,;", "");
        auto edited = puzzleq::readBand (processor.editApvts(), editorBand);
        if (toks.size() > 0) edited.frequencyHz = toks[0].getFloatValue();
        if (toks.size() > 1) edited.gainDb = toks[1].getFloatValue();
        if (toks.size() > 2) edited.q = toks[2].getFloatValue();
        puzzleq::writeBand (processor.editApvts(), editorBand, edited);
        editor.reset();
    };
    editor->onFocusLost = [this] { editor.reset(); };
    addAndMakeVisible (*editor);
    editor->grabKeyboardFocus();
}

void EqDisplay::grabSpectrumPeakAt (juce::Point<float> pos)
{
    const int bin = hitTestSpectrumPeak (pos);
    if (bin < 0 || postMag.size() < 8)
        return;
    const int n = (static_cast<int> (postMag.size()) - 1) * 2;
    const float sr = processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    const float y0 = postMag[static_cast<size_t> (bin - 1)];
    const float y1 = postMag[static_cast<size_t> (bin)];
    const float y2 = postMag[static_cast<size_t> (bin + 1)];
    const float delta = puzzleq::SpectrumAnalyzer::parabolicDelta (y0, y1, y2);
    const float hz = juce::jlimit (puzzleq::kMinHz, puzzleq::kMaxHz,
                                   (static_cast<float> (bin) + delta) * sr / static_cast<float> (n));

    int L = bin, R = bin;
    while (L > 2 && postMag[static_cast<size_t> (L)] > y1 - 6.0f)
        --L;
    while (R < static_cast<int> (postMag.size()) - 2 && postMag[static_cast<size_t> (R)] > y1 - 6.0f)
        ++R;
    const float f1 = puzzleq::SpectrumAnalyzer::binToHz (std::max (1, L), n, sr);
    const float f2 = puzzleq::SpectrumAnalyzer::binToHz (R, n, sr);
    const float q = juce::jlimit (puzzleq::kMinQ, 12.0f, hz / std::max (f2 - f1, hz * 0.08f));

    addBandAt (hz, 0.0f);
    if (primarySel >= 0)
    {
        auto st = puzzleq::readBand (processor.editApvts(), primarySel);
        st.q = q;
        puzzleq::writeBand (processor.editApvts(), primarySel, st);
    }
}

void EqDisplay::beginSketch()
{
    sketching = true;
    sketchPath.clear();
    sketchPts.clear();
}

void EqDisplay::commitSketch()
{
    if (sketchPts.size() < 4)
    {
        sketching = false;
        sketchPath.clear();
        sketchPts.clear();
        return;
    }

    std::sort (sketchPts.begin(), sketchPts.end(),
               [] (const juce::Point<float>& a, const juce::Point<float>& b) { return a.x < b.x; });

    std::vector<float> hz, db;
    hz.reserve (48);
    db.reserve (48);
    for (int i = 0; i < 48; ++i)
    {
        const float t = static_cast<float> (i) / 47.0f;
        const float f = kMinPlotHz * std::pow (kMaxPlotHz / kMinPlotHz, t);
        const float x = hzToX (f);
        float y = sketchPts.front().y;
        if (x <= sketchPts.front().x)
            y = sketchPts.front().y;
        else if (x >= sketchPts.back().x)
            y = sketchPts.back().y;
        else
        {
            for (size_t k = 1; k < sketchPts.size(); ++k)
            {
                if (x <= sketchPts[k].x)
                {
                    const float u = (x - sketchPts[k - 1].x)
                                  / std::max (1.0f, sketchPts[k].x - sketchPts[k - 1].x);
                    y = sketchPts[k - 1].y + u * (sketchPts[k].y - sketchPts[k - 1].y);
                    break;
                }
            }
        }
        hz.push_back (f);
        db.push_back (yToDb (y));
    }

    auto bands = processor.editTarget().uiBands;
    const int n = processor.engine.matcher().fitTargetCurve (hz, db, bands, 8);
    auto& ap = processor.editApvts();
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
        if (bands[static_cast<size_t> (i)].active)
            puzzleq::writeBand (ap, i, bands[static_cast<size_t> (i)]);
    processor.editTarget().pullStateFromApvts();
    (void) n;

    sketching = false;
    sketchPath.clear();
    sketchPts.clear();
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    editor.reset();

    if (e.mods.isAltDown())
    {
        const int hit = hitTestBand (e.position);
        if (auto* solo = processor.apvts.getParameter (puzzleq::pid::soloBand))
        {
            solo->beginChangeGesture();
            const int cur = juce::roundToInt (solo->convertFrom0to1 (solo->getValue()));
            const int next = (hit >= 0 && cur != hit) ? hit : -1;
            solo->setValueNotifyingHost (solo->convertTo0to1 (static_cast<float> (next)));
            solo->endChangeGesture();
        }
        return;
    }

    if (e.mods.isShiftDown() && ! e.mods.isCommandDown())
    {
        beginSketch();
        sketchPath.startNewSubPath (e.position);
        sketchPts.push_back (e.position);
        return;
    }

    const int hit = hitTestBand (e.position);
    if (hit >= 0)
    {
        if (e.mods.isCommandDown())
        {
            auto it = std::find (selection.begin(), selection.end(), hit);
            if (it == selection.end())
                selection.push_back (hit);
            else
                selection.erase (it);
            primarySel = hit;
            if (onSelectionChanged)
                onSelectionChanged();
        }
        else
        {
            setSelectedBand (hit);
        }
        dragBand = hit;
        dragStartQ = puzzleq::readBand (processor.editApvts(), hit).q;
        dragOrigins.clear();
        dragOriginHz = std::max (8.0f, xToHz (e.position.x));
        dragOriginDb = yToDb (e.position.y);
        for (int b : selection)
        {
            auto st = puzzleq::readBand (processor.editApvts(), b);
            dragOrigins.push_back ({ b, st.frequencyHz, st.gainDb, st.q });
        }

        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            for (int s = 0; s < static_cast<int> (puzzleq::FilterShape::NumShapes); ++s)
                m.addItem (s + 1, puzzleq::kShapeNames[s]);
            m.addSeparator();
            m.addItem (100, "Make Dynamic");
            m.addItem (101, "Make Spectral");
            m.addItem (102, "Delete");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                             [this, hit] (int result)
                             {
                                 if (result <= 0)
                                     return;
                                 auto& ap = processor.editApvts();
                                 auto st = puzzleq::readBand (ap, hit);
                                 if (result <= 10)
                                     st.shape = static_cast<puzzleq::FilterShape> (result - 1);
                                 else if (result == 100)
                                     st.dynRangeDb = st.dynRangeDb == 0.0f ? -6.0f : st.dynRangeDb;
                                 else if (result == 101)
                                     st.spectral = true;
                                 else if (result == 102)
                                 {
                                     puzzleq::clearBand (ap, hit);
                                     setSelectedBand (-1);
                                     return;
                                 }
                                 puzzleq::writeBand (ap, hit, st);
                             });
        }
        return;
    }

    if (e.mods.isCommandDown())
    {
        grabSpectrumPeakAt (e.position);
        return;
    }
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (sketching)
    {
        sketchPath.lineTo (e.position);
        sketchPts.push_back (e.position);
        repaint();
        return;
    }
    if (dragBand < 0 || dragOrigins.empty())
        return;

    auto& ap = processor.editApvts();
    const bool qGesture = e.mods.isMiddleButtonDown() || e.mods.isRightButtonDown();
    if (dragOrigins.size() == 1)
    {
        updateBandFromDrag (dragBand, e.position, e.mods.isShiftDown());
        if (qGesture)
        {
            auto st = puzzleq::readBand (ap, dragBand);
            st.q = juce::jlimit (puzzleq::kMinQ, puzzleq::kMaxQ,
                                 dragStartQ * std::pow (2.0f, e.getDistanceFromDragStartY() * -0.01f));
            puzzleq::writeBand (ap, dragBand, st);
        }
        return;
    }

    const float newHz = std::max (8.0f, xToHz (e.position.x));
    const float ratio = newHz / dragOriginHz;
    const float dDb = yToDb (e.position.y) - dragOriginDb;
    const float qMul = qGesture ? std::pow (2.0f, e.getDistanceFromDragStartY() * -0.01f) : 1.0f;
    for (const auto& o : dragOrigins)
    {
        auto st = puzzleq::readBand (ap, o.index);
        float hz = o.hz * ratio;
        if (e.mods.isShiftDown() || processor.uiGlobal.pianoRoll)
            hz = puzzleq::noteToHz (puzzleq::hzToNearestNote (hz));
        st.frequencyHz = juce::jlimit (puzzleq::kMinHz, puzzleq::kMaxHz, hz);
        if (puzzleq::shapeUsesGain (st.shape))
            st.gainDb = juce::jlimit (puzzleq::kMinGainDb, puzzleq::kMaxGainDb, o.gain + dDb);
        if (qGesture)
            st.q = juce::jlimit (puzzleq::kMinQ, puzzleq::kMaxQ, o.q * qMul);
        puzzleq::writeBand (ap, o.index, st);
    }
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (sketching)
        commitSketch();
    dragBand = -1;
    dragOrigins.clear();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitTestBand (e.position);
    if (hit >= 0)
        showValueEditor (hit, e.getPosition());
    else
        addBandAt (xToHz (e.position.x), yToDb (e.position.y));
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int hit = hitTestBand (e.position);
    if (hit < 0)
        return;
    auto& ap = processor.editApvts();
    const auto targets = selection.empty() ? std::vector<int> { hit } : selection;
    const bool applyAll = std::find (targets.begin(), targets.end(), hit) != targets.end();
    for (int b : (applyAll ? targets : std::vector<int> { hit }))
    {
        auto st = puzzleq::readBand (ap, b);
        if (puzzleq::shapeUsesQ (st.shape))
            st.q = juce::jlimit (puzzleq::kMinQ, puzzleq::kMaxQ, st.q * std::pow (1.15f, wheel.deltaY * 8.0f));
        else
            st.slopeDbOct = juce::jlimit (puzzleq::kMinSlope, puzzleq::kMaxSlope, st.slopeDbOct + wheel.deltaY * 12.0f);
        puzzleq::writeBand (ap, b, st);
    }
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    hoverPos = e.position;
    hoverBand = hitTestBand (e.position);
    repaint();
}

bool EqDisplay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        for (int b : selection)
            puzzleq::clearBand (processor.editApvts(), b);
        setSelectedBand (-1);
        return true;
    }
    if (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S')
    {
        if (auto* solo = processor.apvts.getParameter (puzzleq::pid::soloBand))
        {
            solo->beginChangeGesture();
            const int next = primarySel;
            solo->setValueNotifyingHost (solo->convertTo0to1 (static_cast<float> (next)));
            solo->endChangeGesture();
        }
        return true;
    }
    return false;
}

void EqDisplay::paintGrid (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    const auto grid = lf ? lf->grid : juce::Colour (0xff2a3142);
    const auto muted = lf ? lf->muted : juce::Colour (0xff8b95a8);

    g.setColour (grid);
    const float freqs[] = { 20, 30, 40, 50, 60, 80, 100, 200, 300, 400, 500, 800,
                            1000, 2000, 3000, 4000, 5000, 8000, 10000, 16000, 20000 };
    for (float f : freqs)
    {
        const float x = hzToX (f);
        const bool major = (static_cast<int> (f) == 20 || static_cast<int> (f) == 100
                            || static_cast<int> (f) == 1000 || static_cast<int> (f) == 10000);
        g.setColour (major ? grid.brighter (0.25f) : grid);
        g.drawVerticalLine (juce::roundToInt (x), r.getY(), r.getBottom());
        if (major)
        {
            g.setColour (muted);
            g.setFont (10.0f);
            juce::String label = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String (static_cast<int> (f));
            g.drawText (label, juce::Rectangle<float> (x - 16.0f, r.getBottom() + 1.0f, 32.0f, 14.0f),
                        juce::Justification::centred);
        }
    }

    const int range = processor.uiGlobal.displayRangeDb;
    for (int db = -range; db <= range; db += std::max (1, range / 3))
    {
        const float y = dbToY (static_cast<float> (db));
        g.setColour (db == 0 ? grid.brighter (0.4f) : grid);
        g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
        g.setColour (muted);
        g.setFont (10.0f);
        g.drawText (juce::String (db), juce::Rectangle<float> (2.0f, y - 7.0f, 28.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
}

void EqDisplay::paintPiano (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (! processor.uiGlobal.pianoRoll)
        return;
    const float y0 = r.getBottom() - 14.0f;
    for (int n = 24; n <= 108; ++n)
    {
        const float hz = puzzleq::noteToHz (n);
        const float x0 = hzToX (hz);
        const float x1 = hzToX (puzzleq::noteToHz (n + 1));
        const int pc = n % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        g.setColour (black ? juce::Colour (0x33202028) : juce::Colour (0x18ffffff));
        g.fillRect (x0, y0, std::max (1.0f, x1 - x0 - 0.5f), 14.0f);
    }
}

float EqDisplay::specDbToY (juce::Rectangle<float> r, float db) const
{
    const float range = std::max (6.0f, processor.uiGlobal.analyzerRangeDb);
    const float t = juce::jlimit (0.0f, 1.0f, (db + range) / range);
    return r.getBottom() - t * r.getHeight();
}

void EqDisplay::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r,
                               const std::vector<float>& mag, juce::Colour c, bool)
{
    if (mag.size() < 8)
        return;
    const int n = (static_cast<int> (mag.size()) - 1) * 2;
    const float sr = processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    juce::Path p;
    bool started = false;
    for (int i = 1; i < static_cast<int> (mag.size()); ++i)
    {
        const float hz = puzzleq::SpectrumAnalyzer::binToHz (i, n, sr);
        if (hz < kMinPlotHz || hz > kMaxPlotHz)
            continue;
        const float x = hzToX (hz);
        const float y = juce::jlimit (r.getY(), r.getBottom(), specDbToY (r, mag[static_cast<size_t> (i)]));
        if (! started)
        {
            p.startNewSubPath (x, r.getBottom());
            p.lineTo (x, y);
            started = true;
        }
        else
        {
            p.lineTo (x, y);
        }
    }
    if (! started)
        return;
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();
    g.setColour (c);
    g.fillPath (p);
}

void EqDisplay::paintSpectrumLine (juce::Graphics& g, juce::Rectangle<float> r,
                                   const std::vector<float>& mag, juce::Colour c)
{
    if (mag.size() < 8)
        return;
    const int n = (static_cast<int> (mag.size()) - 1) * 2;
    const float sr = processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    juce::Path p;
    bool started = false;
    for (int i = 1; i < static_cast<int> (mag.size()); i += 2)
    {
        const float hz = puzzleq::SpectrumAnalyzer::binToHz (i, n, sr);
        if (hz < kMinPlotHz || hz > kMaxPlotHz)
            continue;
        const float x = hzToX (hz);
        const float y = juce::jlimit (r.getY(), r.getBottom(), specDbToY (r, mag[static_cast<size_t> (i)]));
        if (! started) { p.startNewSubPath (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (started)
    {
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.1f));
    }
}

void EqDisplay::paintCollision (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (overlayMag.size() < 8 || postMag.size() != overlayMag.size())
        return;
    const int n = (static_cast<int> (postMag.size()) - 1) * 2;
    const float sr = processor.getSampleRate() > 0 ? static_cast<float> (processor.getSampleRate()) : 48000.0f;
    juce::Path p;
    bool started = false;
    for (int i = 1; i < static_cast<int> (postMag.size()); ++i)
    {
        const float a = postMag[static_cast<size_t> (i)];
        const float b = overlayMag[static_cast<size_t> (i)];
        if (b < a + 2.5f || b < -48.0f)
            continue;
        const float hz = puzzleq::SpectrumAnalyzer::binToHz (i, n, sr);
        if (hz < kMinPlotHz || hz > kMaxPlotHz)
            continue;
        const float x = hzToX (hz);
        const float y = specDbToY (r, b);
        if (! started) { p.startNewSubPath (x, r.getBottom()); p.lineTo (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (! started)
        return;
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();
    g.setColour (juce::Colour (0x55ff6b3d));
    g.fillPath (p);
}

void EqDisplay::paintCurve (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    const auto curveC = lf ? lf->curve : juce::Colour (0xff4de3c1);

    juce::Path curve;
    const int steps = std::max (64, getWidth() / 4);
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (steps);
        const float hz = kMinPlotHz * std::pow (kMaxPlotHz / kMinPlotHz, t);
        const float db = processor.editTarget().engine.compositeMagnitudeDb (hz);
        const float x = r.getX() + t * r.getWidth();
        const float y = dbToY (db);
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    g.setColour (curveC);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void EqDisplay::paintBandGhost (juce::Graphics& g, juce::Rectangle<float> r, int band)
{
    if (band < 0)
        return;
    juce::Path ghost;
    const int steps = std::max (48, getWidth() / 8);
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (steps);
        const float hz = kMinPlotHz * std::pow (kMaxPlotHz / kMinPlotHz, t);
        const float db = processor.editTarget().engine.bandMagnitudeDb (band, hz);
        const float x = r.getX() + t * r.getWidth();
        const float y = dbToY (db);
        if (i == 0) ghost.startNewSubPath (x, y);
        else ghost.lineTo (x, y);
    }
    g.setColour (juce::Colour (0x66ffc36b));
    g.strokePath (ghost, juce::PathStrokeType (1.4f));
}

void EqDisplay::paintReadout (juce::Graphics& g)
{
    if (! getLocalBounds().contains (hoverPos.roundToInt()))
        return;
    const float hz = xToHz (hoverPos.x);
    const float db = yToDb (hoverPos.y);
    const int note = puzzleq::hzToNearestNote (hz);
    const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc = ((note % 12) + 12) % 12;
    const int oct = note / 12 - 1;
    const auto plot = plotBounds();
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawVerticalLine (juce::roundToInt (hoverPos.x), plot.getY(), plot.getBottom());
    g.setColour (juce::Colour (0xee0c0e13));
    auto box = juce::Rectangle<float> (hoverPos.x + 10.0f, hoverPos.y - 28.0f, 168.0f, 24.0f);
    if (box.getRight() > static_cast<float> (getWidth() - 8))
        box = box.withX (hoverPos.x - 178.0f);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (juce::Colour (0xffe7ecf4));
    g.setFont (11.0f);
    juce::String hzText = hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (hz, 1) + " Hz";
    g.drawText (hzText + "   " + juce::String (db, 1) + " dB   " + names[pc] + juce::String (oct),
                box.reduced (6.0f, 0), juce::Justification::centredLeft);
}

void EqDisplay::paintHandles (juce::Graphics& g)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    const auto handleC = lf ? lf->handle : juce::Colour (0xffffc36b);
    const auto muted = lf ? lf->muted : juce::Colour (0xff8b95a8);
    const auto accent = lf ? lf->accent : juce::Colour (0xff7c5cff);

    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        const auto& b = processor.editTarget().uiBands[static_cast<size_t> (i)];
        if (! b.active)
            continue;
        const auto p = bandToPoint (b);
        const bool sel = std::find (selection.begin(), selection.end(), i) != selection.end();
        const float rad = sel ? 7.0f : 5.5f;
        g.setColour (b.enabled ? (sel ? handleC : accent) : muted);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f, 1.0f);

        if (std::abs (b.dynRangeDb) > 0.01f)
        {
            const float extra = processor.editTarget().engine.dynamicGainDb (i);
            const float ring = rad + 3.0f + std::abs (extra) * 0.25f;
            g.setColour (juce::Colour (0xffff6b7a).withAlpha (0.85f));
            g.drawEllipse (p.x - ring, p.y - ring, ring * 2.0f, ring * 2.0f, 1.4f);
        }
        if (b.spectral)
        {
            g.setColour (juce::Colour (0xff7c5cff));
            g.drawEllipse (p.x - rad - 5.0f, p.y - rad - 5.0f, (rad + 5.0f) * 2.0f, (rad + 5.0f) * 2.0f, 1.0f);
        }
    }
}

void EqDisplay::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    const auto bg = lf ? lf->panel : juce::Colour (0xff141821);
    const auto spec = lf ? lf->spectrum : juce::Colour (0x664a5d8a);
    const auto specPre = lf ? lf->spectrumPre : juce::Colour (0x3338c9a7);

    auto r = getLocalBounds().toFloat();
    g.setColour (bg);
    g.fillRoundedRectangle (r, 8.0f);

    const auto plot = plotBounds();
    paintGrid (g, plot);
    paintPiano (g, plot);

    if (processor.uiGlobal.analyzerPre)
        paintSpectrum (g, plot, preMag, specPre, true);
    paintSpectrum (g, plot, postMag, spec, true);
    paintSpectrumLine (g, plot, postPeaks, juce::Colour (0x66ffffff));
    paintCollision (g, plot);

    paintCurve (g, plot);
    paintBandGhost (g, plot, primarySel);
    paintHandles (g);
    paintReadout (g);

    if (sketching && ! sketchPath.isEmpty())
    {
        g.setColour (juce::Colour (0x887c5cff));
        g.strokePath (sketchPath, juce::PathStrokeType (1.6f));
    }

    g.setColour ((lf ? lf->muted : juce::Colour (0xff8b95a8)));
    g.setFont (11.0f);
    const juce::String hint = hoverBand >= 0
        ? ("Band " + juce::String (hoverBand + 1) + "  "
           + juce::String (processor.editTarget().uiBands[static_cast<size_t> (hoverBand)].frequencyHz, 1) + " Hz")
        : (processor.isEditingRemote()
               ? ("Editing " + processor.editTarget().instanceName + "  ·  Double-click add  ·  Shift-drag sketch  ·  Cmd-click grab")
               : "Double-click to add  ·  Shift-drag to sketch  ·  Cmd-click spectrum grab  ·  Alt-click solo");
    g.drawText (hint, getLocalBounds().removeFromTop (16).reduced (10, 0), juce::Justification::centredLeft);
}

void EqDisplay::resized()
{
    if (editor != nullptr)
        editor->setTopLeftPosition (8, 8);
}

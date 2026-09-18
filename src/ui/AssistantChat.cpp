#include "ui/AssistantChat.h"
#include "ui/PuzzlLookAndFeel.h"
#include "assistant/EqLlm.h"
#include "assistant/EqAnalyze.h"
#include "state/ParameterLayout.h"
#include "state/Presets.h"
#include <algorithm>

namespace {
int firstFree (const std::array<puzzleq::BandState, puzzleq::kMaxBands>& bands)
{
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
        if (! bands[static_cast<size_t> (i)].active)
            return i;
    return -1;
}

int existingSimilar (const std::array<puzzleq::BandState, puzzleq::kMaxBands>& bands,
                     const puzzleq::BandState& want)
{
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        const auto& b = bands[static_cast<size_t> (i)];
        if (! b.active || b.shape != want.shape)
            continue;
        const float ratio = want.frequencyHz > 0.0f ? b.frequencyHz / want.frequencyHz : 1.0f;
        if (ratio > 0.7f && ratio < 1.4f)
            return i;
    }
    return -1;
}
} // namespace

AssistantChat::AssistantChat (PuzzlEqAudioProcessor& proc)
    : processor (proc)
{
    title.setText ("EQ Chat", juce::dontSendNotification);
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    status.setText ("Analyzes the live spectrum first. Hebrew + English. Optional free Ollama / Groq / Gemini.", juce::dontSendNotification);
    status.setFont (juce::FontOptions (11.0f));
    status.setColour (juce::Label::textColourId, juce::Colour (0xff8b95a8));

    log.setMultiLine (true, true);
    log.setReadOnly (true);
    log.setScrollbarsShown (true);
    log.setFont (juce::FontOptions (13.0f));
    log.setText ("Ask for an EQ move in English or Hebrew.\n"
                 "Examples: roll off the low end / clean mud / make the band narrower\n"
                 "HE: \xd7\xaa\xd7\x95\xd7\xa8\xd7\x99\xd7\x93 \xd7\x91 500 \xd7\x94\xd7\xa8\xd7\xa5 3 \xd7\x93\xd7\x99\xd7\x91\xd7\x99"
                 "  \xc2\xb7  \xd7\xaa\xd7\xa2\xd7\xa9\xd7\x94 \xd7\x90\xd7\xaa \xd7\x94\xd7\x91\xd7\x90\xd7\xa0\xd7\x93 \xd7\xa6\xd7\xa8 \xd7\x99\xd7\x95\xd7\xaa\xd7\xa8\n");

    input.setMultiLine (false);
    input.setReturnKeyStartsNewLine (false);
    input.setFont (juce::FontOptions (14.0f));
    input.setTextToShowWhenEmpty ("Tell PuzzlEQ what to do...", juce::Colour (0xff8b95a8));
    input.addListener (this);

    send.onClick = [this] { sendCurrent(); };
    settingsBtn.setClickingTogglesState (true);
    settingsBtn.onClick = [this] { showSettings (settingsBtn.getToggleState()); };

    auto bindChip = [this] (juce::TextButton& b, const char* phrase)
    {
        b.onClick = [this, phrase]
        {
            input.setText (phrase, juce::dontSendNotification);
            sendCurrent();
        };
    };
    bindChip (chipLows, "roll off the low end");
    bindChip (chipMud, "clean mud");
    bindChip (chipPresence, "boost presence");
    bindChip (chipAir, "add air");

    groqLbl.setText ("Groq key (free Llama)", juce::dontSendNotification);
    geminiLbl.setText ("Gemini key (free Flash)", juce::dontSendNotification);
    ollamaLbl.setText ("Ollama URL", juce::dontSendNotification);
    groqKey.setPasswordCharacter (static_cast<juce::juce_wchar> (0x2022));
    geminiKey.setPasswordCharacter (static_cast<juce::juce_wchar> (0x2022));
    auto s = puzzleq::loadLlmSettings();
    groqKey.setText (s.groqKey, juce::dontSendNotification);
    geminiKey.setText (s.geminiKey, juce::dontSendNotification);
    ollamaUrl.setText (s.ollamaUrl, juce::dontSendNotification);

    groqLbl.setVisible (false);
    geminiLbl.setVisible (false);
    ollamaLbl.setVisible (false);
    groqKey.setVisible (false);
    geminiKey.setVisible (false);
    ollamaUrl.setVisible (false);

    for (auto* c : std::initializer_list<juce::Component*> {
             &title, &status, &log, &input, &send, &settingsBtn,
             &chipLows, &chipMud, &chipPresence, &chipAir,
             &groqLbl, &geminiLbl, &ollamaLbl, &groqKey, &geminiKey, &ollamaUrl })
        addAndMakeVisible (c);
}

AssistantChat::~AssistantChat()
{
    alive = false;
    if (worker != nullptr && worker->joinable())
        worker->join();
}

void AssistantChat::showSettings (bool on)
{
    settingsOpen = on;
    groqLbl.setVisible (on);
    geminiLbl.setVisible (on);
    ollamaLbl.setVisible (on);
    groqKey.setVisible (on);
    geminiKey.setVisible (on);
    ollamaUrl.setVisible (on);
    if (! on)
    {
        puzzleq::LlmSettings s = puzzleq::loadLlmSettings();
        s.groqKey = groqKey.getText().toStdString();
        s.geminiKey = geminiKey.getText().toStdString();
        if (ollamaUrl.getText().isNotEmpty())
            s.ollamaUrl = ollamaUrl.getText().toStdString();
        puzzleq::saveLlmSettings (s);
    }
    resized();
}

void AssistantChat::paint (juce::Graphics& g)
{
    auto* lf = dynamic_cast<PuzzlLookAndFeel*> (&getLookAndFeel());
    g.setColour (lf ? lf->panel : juce::Colour (0xff141821));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
}

void AssistantChat::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto head = r.removeFromTop (22);
    title.setBounds (head.removeFromLeft (90));
    settingsBtn.setBounds (head.removeFromRight (48));
    r.removeFromTop (2);
    status.setBounds (r.removeFromTop (32));
    if (settingsOpen)
    {
        groqLbl.setBounds (r.removeFromTop (16));
        groqKey.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        geminiLbl.setBounds (r.removeFromTop (16));
        geminiKey.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        ollamaLbl.setBounds (r.removeFromTop (16));
        ollamaUrl.setBounds (r.removeFromTop (22));
        r.removeFromTop (8);
    }
    auto chips = r.removeFromTop (52);
    auto row1 = chips.removeFromTop (24);
    chips.removeFromTop (4);
    auto row2 = chips.removeFromTop (24);
    const int cw1 = juce::jmax (40, (row1.getWidth() - 4) / 2);
    chipLows.setBounds (row1.removeFromLeft (cw1));
    row1.removeFromLeft (4);
    chipMud.setBounds (row1);
    const int cw2 = juce::jmax (40, (row2.getWidth() - 4) / 2);
    chipPresence.setBounds (row2.removeFromLeft (cw2));
    row2.removeFromLeft (4);
    chipAir.setBounds (row2);
    r.removeFromTop (8);
    auto row = r.removeFromBottom (28);
    send.setBounds (row.removeFromRight (64));
    row.removeFromRight (6);
    input.setBounds (row);
    r.removeFromBottom (8);
    log.setBounds (r);
}

void AssistantChat::textEditorReturnKeyPressed (juce::TextEditor&)
{
    sendCurrent();
}

void AssistantChat::appendLine (const juce::String& who, const juce::String& text, juce::Colour colour)
{
    log.moveCaretToEnd();
    log.setColour (juce::TextEditor::textColourId, colour);
    log.insertTextAtCaret (who + ": " + text + "\n");
    log.setColour (juce::TextEditor::textColourId, juce::Colour (0xffe7ecf4));
    log.moveCaretToEnd();
}

void AssistantChat::applyPlan (const puzzleq::ChatPlan& plan)
{
    auto& target = processor.editTarget();
    target.undo.beginNewTransaction ("EQ Chat");
    int lastSlot = -1;
    for (const auto& op : plan.ops)
    {
        if (op.kind == puzzleq::ChatOp::Kind::Help)
            continue;
        if (op.kind == puzzleq::ChatOp::Kind::ClearAll)
        {
            for (int i = 0; i < puzzleq::kMaxBands; ++i)
            {
                puzzleq::clearBand (target.apvts, i);
                target.uiBands[static_cast<size_t> (i)] = {};
            }
            continue;
        }
        if (op.kind == puzzleq::ChatOp::Kind::ApplyPreset)
        {
            puzzleq::applyPreset (target.apvts, op.presetIndex);
            target.pullStateFromApvts (true);
            continue;
        }
        if (op.kind == puzzleq::ChatOp::Kind::TweakQ)
        {
            int slot = op.targetBand;
            if (slot < 0 || slot >= puzzleq::kMaxBands
                || ! target.uiBands[static_cast<size_t> (slot)].active)
            {
                slot = -1;
                for (int i = puzzleq::kMaxBands - 1; i >= 0; --i)
                    if (target.uiBands[static_cast<size_t> (i)].active)
                    {
                        slot = i;
                        break;
                    }
            }
            if (slot < 0)
                continue;
            auto b = target.uiBands[static_cast<size_t> (slot)];
            b.q = juce::jlimit (puzzleq::kMinQ, puzzleq::kMaxQ, b.q * (op.qMul > 0.05f ? op.qMul : 1.28f));
            target.uiBands[static_cast<size_t> (slot)] = b;
            puzzleq::writeBand (target.apvts, slot, b);
            lastSlot = slot;
            continue;
        }
        if (op.kind == puzzleq::ChatOp::Kind::AddOrUpdate)
        {
            int slot = existingSimilar (target.uiBands, op.band);
            if (slot < 0)
                slot = firstFree (target.uiBands);
            if (slot < 0)
                continue;
            target.uiBands[static_cast<size_t> (slot)] = op.band;
            puzzleq::writeBand (target.apvts, slot, op.band);
            lastSlot = slot;
        }
    }
    target.pullStateFromApvts (true);
    target.engine.setBands (target.uiBands);
    target.markLocalEdit();
    target.commitUiBandsToHost();
    if (onApplied)
        onApplied (lastSlot);
}

void AssistantChat::sendCurrent()
{
    const auto text = input.getText().trim();
    if (text.isEmpty() || busy.load())
        return;
    input.clear();
    appendLine ("You", text, juce::Colour (0xffffc36b));

    puzzleq::ChatContext ctx;
    ctx.selectedBand = getSelectedBand ? getSelectedBand() : -1;
    ctx.bands = processor.editTarget().uiBands;

    auto& an = processor.engine.analyzer();
    std::vector<float> mag, peaks, postPeaks;
    if (! an.copyCurrent (mag, true))
        an.copyCurrent (mag, false);
    if (! an.copyPeaks (peaks, true))
        an.copyPeaks (peaks, false);
    if (an.copyPeaks (postPeaks, false) && postPeaks.size() == peaks.size())
        for (size_t i = 0; i < peaks.size(); ++i)
            peaks[i] = std::max (peaks[i], postPeaks[i]);
    ctx.spectrum = puzzleq::analyzeSpectrum (mag, peaks, an.sampleRate(), an.fftSize());

    if (ctx.spectrum.ok)
    {
        status.setText ("Analyzed live FFT " + juce::String (ctx.spectrum.fftSize)
                            + "  ·  harsh " + juce::String (ctx.spectrum.harshHz, 0) + " Hz",
                        juce::dontSendNotification);
        appendLine ("Analyze", juce::String (ctx.spectrum.summary), juce::Colour (0xff8b95a8));
    }
    else
    {
        status.setText ("No spectrum yet — play audio for a full analyze.", juce::dontSendNotification);
    }

    auto local = puzzleq::parseEqChat (text.toStdString(), ctx);
    if (local.understood)
    {
        applyPlan (local);
        appendLine ("PuzzlEQ", juce::String (local.reply), juce::Colour (0xff4de3c1));
        if (! ctx.spectrum.ok)
            status.setText ("Local (no API)", juce::dontSendNotification);
        return;
    }

    busy = true;
    status.setText ("Analyzed. Asking a free model...", juce::dontSendNotification);
    appendLine ("PuzzlEQ", "Not a built-in phrase. Trying a free model with the spectrum report...", juce::Colour (0xff8b95a8));

    puzzleq::LlmSettings settings = puzzleq::loadLlmSettings();
    settings.groqKey = groqKey.getText().toStdString();
    settings.geminiKey = geminiKey.getText().toStdString();
    if (ollamaUrl.getText().isNotEmpty())
        settings.ollamaUrl = ollamaUrl.getText().toStdString();
    puzzleq::saveLlmSettings (settings);

    if (worker != nullptr && worker->joinable())
        worker->join();

    const auto prompt = text.toStdString();
    const auto analysis = ctx.spectrum.summary;
    worker = std::make_unique<std::thread> ([this, prompt, settings, local, analysis]()
    {
        const auto llm = puzzleq::requestEqLlm (prompt, settings, analysis);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AssistantChat> (this), llm, local]()
        {
            if (safe == nullptr || ! safe->alive.load())
                return;
            safe->busy = false;
            if (! llm.ok)
            {
                safe->appendLine ("PuzzlEQ",
                            juce::String (local.reply) + "\n" + juce::String (llm.error),
                            juce::Colour (0xffff6b7a));
                safe->status.setText ("Local only", juce::dontSendNotification);
                return;
            }
            auto plan = puzzleq::parseEqChatJson (llm.text);
            if (plan.understood)
                safe->applyPlan (plan);
            safe->appendLine ("PuzzlEQ",
                        juce::String (llm.source) + " - " + juce::String (plan.reply),
                        juce::Colour (0xff4de3c1));
            safe->status.setText (juce::String (llm.source), juce::dontSendNotification);
        });
    });
}

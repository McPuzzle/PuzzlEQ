#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "state/Presets.h"
#include <algorithm>
#include <array>
#include <cmath>

std::mutex PuzzlEqAudioProcessor::registryMutex;
std::vector<PuzzlEqAudioProcessor*> PuzzlEqAudioProcessor::registry;

PuzzlEqAudioProcessor::PuzzlEqAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "PuzzlEQ", makeLayout())
{
    ccMap.fill (-1);
    instanceName = "PuzzlEQ " + juce::String (juce::Random::getSystemRandom().nextInt (900) + 100);
    std::lock_guard<std::mutex> lock (registryMutex);
    registry.push_back (this);
}

PuzzlEqAudioProcessor::~PuzzlEqAudioProcessor()
{
    std::lock_guard<std::mutex> lock (registryMutex);
    for (auto* p : registry)
        if (p->overlayInstance == this)
            p->overlayInstance = nullptr;
    registry.erase (std::remove (registry.begin(), registry.end(), this), registry.end());
}

std::vector<PuzzlEqAudioProcessor*> PuzzlEqAudioProcessor::allInstances()
{
    std::lock_guard<std::mutex> lock (registryMutex);
    return registry;
}

void PuzzlEqAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (static_cast<float> (sampleRate), samplesPerBlock);
    setLatencySamples (engine.latencySamples());
    outputPeakL = outputPeakR = 0.0f;
}

void PuzzlEqAudioProcessor::releaseResources()
{
    engine.reset();
}

bool PuzzlEqAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn.isDisabled() || mainOut.isDisabled())
        return false;
    if (mainIn.size() < 1 || mainOut.size() < 1)
        return false;
    if (mainIn != mainOut)
        return false;
    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

void PuzzlEqAudioProcessor::markLocalEdit()
{
    suppressHostPullUntilMs = juce::Time::getMillisecondCounterHiRes() + 1200.0;
}

bool PuzzlEqAudioProcessor::shouldPullFromHost() const
{
    return juce::Time::getMillisecondCounterHiRes() >= suppressHostPullUntilMs;
}

void PuzzlEqAudioProcessor::commitUiBandsToHost()
{
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        const auto have = puzzleq::readBand (apvts, i);
        const auto& want = uiBands[static_cast<size_t> (i)];
        if (want.active != have.active
            || (want.active && (std::abs (want.frequencyHz - have.frequencyHz) > 0.05f
                                || std::abs (want.gainDb - have.gainDb) > 0.02f
                                || std::abs (want.q - have.q) > 0.002f
                                || want.shape != have.shape
                                || want.enabled != have.enabled)))
            puzzleq::writeBand (apvts, i, want);
    }
}

void PuzzlEqAudioProcessor::pullStateFromApvts()
{
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        auto fromHost = puzzleq::readBand (apvts, i);
        auto& local = uiBands[static_cast<size_t> (i)];
        // FL Studio reverts parameter edits when the mouse button comes up.
        // Never blank a band the UI still considers active.
        if (local.active && ! fromHost.active && ! shouldPullFromHost())
            continue;
        local = fromHost;
    }
    uiGlobal = puzzleq::readGlobal (apvts);
}

void PuzzlEqAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController())
            continue;
        const int cc = msg.getControllerNumber();
        const float v = static_cast<float> (msg.getControllerValue()) / 127.0f;
        if (midiLearnId.isNotEmpty())
        {
            if (auto* p = apvts.getParameter (midiLearnId))
            {
                const int idx = p->getParameterIndex();
                ccMap[static_cast<size_t> (cc)] = idx;
            }
            midiLearnId.clear();
        }
        else if (ccMap[static_cast<size_t> (cc)] >= 0)
        {
            if (auto* p = getParameters()[ccMap[static_cast<size_t> (cc)]])
                p->setValueNotifyingHost (v);
        }
    }
    midi.clear();

    // Read APVTS on the audio thread without overwriting uiBands. The editor
    // owns uiBands so a host that is slow to echo parameters cannot blank the
    // handles on the next buffer.
    std::array<puzzleq::BandState, puzzleq::kMaxBands> bands {};
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        const auto& local = uiBands[static_cast<size_t> (i)];
        bands[static_cast<size_t> (i)] = local.active ? local : puzzleq::readBand (apvts, i);
    }
    const auto g = puzzleq::readGlobal (apvts);
    engine.setBands (bands);
    engine.setGlobal (g);
    engine.setSidechainListen (sidechainListen.load(), selectedBandForListen);

    auto main = getBusBuffer (buffer, true, 0);
    float* l = main.getWritePointer (0);
    float* r = main.getNumChannels() > 1 ? main.getWritePointer (1) : l;

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        scL = sc.getReadPointer (0);
        scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL;
    }

    const int n = buffer.getNumSamples();

    float inL = 0.0f, inR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        inL = std::max (inL, std::abs (l[i]));
        inR = std::max (inR, std::abs (r[i]));
    }
    inputPeakL = inL;
    inputPeakR = inR;

    engine.process (l, r, scL, scR, n);
    setLatencySamples (engine.latencySamples());

    // Clear extra output channels
    for (int ch = getMainBusNumInputChannels(); ch < getMainBusNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    float pkL = 0.0f, pkR = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        pkL = std::max (pkL, std::abs (l[i]));
        pkR = std::max (pkR, std::abs (r[i]));
    }
    outputPeakL = pkL;
    outputPeakR = pkR;

    std::vector<float> spec;
    if (engine.analyzer().consume (spec, false))
    {
        std::lock_guard<std::mutex> lock (specLock);
        publishedPost = std::move (spec);
    }
}

void PuzzlEqAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    commitUiBandsToHost();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PuzzlEqAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            suppressHostPullUntilMs = 0.0;
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            pullStateFromApvts();
        }
}

void PuzzlEqAudioProcessor::snapshotToA()
{
    stateA = apvts.copyState();
    showingA = true;
}

void PuzzlEqAudioProcessor::snapshotToB()
{
    stateB = apvts.copyState();
    showingA = false;
}

void PuzzlEqAudioProcessor::toggleAB()
{
    if (showingA)
    {
        stateA = apvts.copyState();
        if (stateB.isValid())
            apvts.replaceState (stateB.createCopy());
        showingA = false;
    }
    else
    {
        stateB = apvts.copyState();
        if (stateA.isValid())
            apvts.replaceState (stateA.createCopy());
        showingA = true;
    }
}

static thread_local std::array<puzzleq::BandState, puzzleq::kMaxBands> gClipboard {};
static thread_local int gClipboardCount = 0;

void PuzzlEqAudioProcessor::copyActiveBands()
{
    gClipboardCount = 0;
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
    {
        auto b = puzzleq::readBand (apvts, i);
        if (b.active)
            gClipboard[static_cast<size_t> (gClipboardCount++)] = b;
    }
}

void PuzzlEqAudioProcessor::copyPublishedSpectrum (std::vector<float>& dest) const
{
    std::lock_guard<std::mutex> lock (specLock);
    dest = publishedPost;
}

int PuzzlEqAudioProcessor::getNumPrograms()
{
    // Hosts require at least one program. Do not expose factory presets here:
    // FL Studio (and others) call setCurrentProgram(0) when the editor opens,
    // which used to apply the empty "Init" preset and wipe every band.
    return 1;
}

void PuzzlEqAudioProcessor::setCurrentProgram (int)
{
}

const juce::String PuzzlEqAudioProcessor::getProgramName (int)
{
    return "PuzzlEQ";
}

void PuzzlEqAudioProcessor::applyFactoryPreset (int index)
{
    suppressHostPullUntilMs = 0.0;
    puzzleq::applyPreset (apvts, index);
    currentProgram = index;
    pullStateFromApvts();
}

void PuzzlEqAudioProcessor::copyBandsFrom (PuzzlEqAudioProcessor& other)
{
    for (int i = 0; i < puzzleq::kMaxBands; ++i)
        puzzleq::writeBand (apvts, i, puzzleq::readBand (other.apvts, i));
}

void PuzzlEqAudioProcessor::pasteBands()
{
    for (int i = 0; i < gClipboardCount; ++i)
    {
        const int slot = puzzleq::findFreeBand (apvts);
        if (slot < 0)
            break;
        puzzleq::writeBand (apvts, slot, gClipboard[static_cast<size_t> (i)]);
    }
}

void PuzzlEqAudioProcessor::beginMidiLearn (const juce::String& paramId)
{
    midiLearnId = paramId;
}

void PuzzlEqAudioProcessor::cancelMidiLearn()
{
    midiLearnId.clear();
}

juce::AudioProcessorEditor* PuzzlEqAudioProcessor::createEditor()
{
    return new PuzzlEqAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PuzzlEqAudioProcessor();
}

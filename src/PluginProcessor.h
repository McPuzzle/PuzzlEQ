#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "state/ParameterLayout.h"
#include "dsp/EqEngine.h"
#include <array>
#include <mutex>
#include <vector>

class PuzzlEqAudioProcessor : public juce::AudioProcessor
{
public:
    PuzzlEqAudioProcessor();
    ~PuzzlEqAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undo;
    puzzleq::EqEngine engine;

    void snapshotToA();
    void snapshotToB();
    void toggleAB();
    bool isShowingA() const noexcept { return showingA; }

    void copyActiveBands();
    void pasteBands();

    void beginMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    bool isLearning() const noexcept { return midiLearnId.isNotEmpty(); }
    juce::String midiLearnParam() const { return midiLearnId; }

    juce::String instanceName;
    static std::vector<PuzzlEqAudioProcessor*> allInstances();

    std::array<puzzleq::BandState, puzzleq::kMaxBands> uiBands {};
    puzzleq::GlobalState uiGlobal {};
    void pullStateFromApvts();

    float outputPeakL = 0.0f, outputPeakR = 0.0f;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() { return puzzleq::createParameterLayout(); }

    juce::ValueTree stateA, stateB;
    bool showingA = true;
    juce::String midiLearnId;
    std::array<int, 128> ccMap {}; // CC -> parameter index, -1 none

    static std::mutex registryMutex;
    static std::vector<PuzzlEqAudioProcessor*> registry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuzzlEqAudioProcessor)
};

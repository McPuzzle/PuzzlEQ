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
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return false; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorParameter* getBypassParameter() const override;

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
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
    void copyBandsFrom (PuzzlEqAudioProcessor& other);
    void applyFactoryPreset (int index);

    void beginMidiLearn (const juce::String& paramId);
    void cancelMidiLearn();
    bool isLearning() const noexcept { return midiLearnId.isNotEmpty(); }
    juce::String midiLearnParam() const { return midiLearnId; }

    juce::String instanceName;
    static std::vector<PuzzlEqAudioProcessor*> allInstances();

    std::array<puzzleq::BandState, puzzleq::kMaxBands> uiBands {};
    puzzleq::GlobalState uiGlobal {};
    void pullStateFromApvts();
    void pullStateFromApvts (bool allowHostDeactivate);
    void commitUiBandsToHost();
    void markLocalEdit();
    bool shouldPullFromHost() const;

    float outputPeakL = 0.0f, outputPeakR = 0.0f;
    float inputPeakL = 0.0f, inputPeakR = 0.0f;

    std::atomic<bool> sidechainListen { false };
    std::atomic<bool> editRemote { false };
    PuzzlEqAudioProcessor* overlayInstance = nullptr;

    void copyPublishedSpectrum (std::vector<float>& dest) const;
    int selectedBandForListen = -1;

    bool isEditingRemote() const noexcept
    {
        return editRemote.load() && overlayInstance != nullptr && overlayInstance != this;
    }

    PuzzlEqAudioProcessor& editTarget() noexcept
    {
        return isEditingRemote() ? *overlayInstance : *this;
    }

    const PuzzlEqAudioProcessor& editTarget() const noexcept
    {
        return isEditingRemote() ? *overlayInstance : *this;
    }

    juce::AudioProcessorValueTreeState& editApvts() noexcept { return editTarget().apvts; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() { return puzzleq::createParameterLayout(); }

    juce::ValueTree stateA, stateB;
    bool showingA = true;
    int currentProgram = 0;
    mutable std::mutex specLock;
    std::vector<float> publishedPost;
    juce::String midiLearnId;
    std::array<int, 128> ccMap {}; // CC -> parameter index, -1 none
    double suppressHostPullUntilMs = 0.0;
    int lastReportedLatency = -1;
    std::vector<float> specScratch;

    static std::mutex registryMutex;
    static std::vector<PuzzlEqAudioProcessor*> registry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PuzzlEqAudioProcessor)
};

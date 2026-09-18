#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "assistant/EqChat.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class AssistantChat : public juce::Component,
                      private juce::TextEditor::Listener
{
public:
    explicit AssistantChat (PuzzlEqAudioProcessor& proc);
    ~AssistantChat() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void focusInput() { input.grabKeyboardFocus(); }

    std::function<void (int lastBand)> onApplied;
    std::function<int()> getSelectedBand;

private:
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void sendCurrent();
    void appendLine (const juce::String& who, const juce::String& text, juce::Colour colour);
    void applyPlan (const puzzleq::ChatPlan& plan);
    void showSettings (bool on);

    PuzzlEqAudioProcessor& processor;
    juce::Label title, status;
    juce::TextEditor log, input, groqKey, geminiKey, ollamaUrl;
    juce::TextButton send { "Send" }, settingsBtn { "API" };
    juce::TextButton chipLows { "Roll off lows" }, chipMud { "Clean mud" },
        chipPresence { "Boost presence" }, chipAir { "Add air" };
    juce::Label groqLbl, geminiLbl, ollamaLbl;
    bool settingsOpen = false;
    std::atomic<bool> busy { false };
    std::atomic<bool> alive { true };
    std::unique_ptr<std::thread> worker;
};

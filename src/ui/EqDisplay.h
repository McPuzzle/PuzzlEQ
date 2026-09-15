#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class EqDisplay : public juce::Component,
                  private juce::Timer
{
public:
    explicit EqDisplay (PuzzlEqAudioProcessor& proc);
    ~EqDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    int selectedBand() const noexcept { return primarySel; }
    void setSelectedBand (int b);
    std::function<void()> onSelectionChanged;

    void grabSpectrumPeakAt (juce::Point<float> pos);
    void beginSketch();
    void commitSketch();

private:
    void timerCallback() override;
    juce::Rectangle<float> plotBounds() const;
    float xToHz (float x) const;
    float hzToX (float hz) const;
    float yToDb (float y) const;
    float dbToY (float db) const;
    juce::Point<float> bandToPoint (const puzzleq::BandState& b) const;
    int hitTestBand (juce::Point<float> p) const;
    int hitTestSpectrumPeak (juce::Point<float> p) const;
    void addBandAt (float hz, float db);
    void updateBandFromDrag (int band, juce::Point<float> p, bool quantize);
    void showValueEditor (int band, juce::Point<int> at);
    void paintGrid (juce::Graphics& g, juce::Rectangle<float> r);
    void paintPiano (juce::Graphics& g, juce::Rectangle<float> r);
    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<float>& mag,
                        juce::Colour c, bool fromBottom);
    void paintSpectrumLine (juce::Graphics& g, juce::Rectangle<float> r, const std::vector<float>& mag, juce::Colour c);
    void paintCollision (juce::Graphics& g, juce::Rectangle<float> r);
    void paintCurve (juce::Graphics& g, juce::Rectangle<float> r);
    void paintBandGhost (juce::Graphics& g, juce::Rectangle<float> r, int band);
    void paintHandles (juce::Graphics& g);
    void paintReadout (juce::Graphics& g);
    float specDbToY (juce::Rectangle<float> r, float db) const;

    PuzzlEqAudioProcessor& processor;
    std::vector<float> preMag, postMag, postPeaks, overlayMag;
    std::vector<int> selection;
    int primarySel = -1;
    int dragBand = -1;
    float dragStartQ = 1.0f;
    struct DragOrigin { int index = -1; float hz = 1000.0f; float gain = 0.0f; float q = 1.0f; };
    std::vector<DragOrigin> dragOrigins;
    float dragOriginHz = 1000.0f;
    float dragOriginDb = 0.0f;
    bool sketching = false;
    juce::Path sketchPath;
    std::vector<juce::Point<float>> sketchPts;
    juce::Point<float> hoverPos;
    int hoverBand = -1;
    std::unique_ptr<juce::TextEditor> editor;
    int editorBand = -1;
};

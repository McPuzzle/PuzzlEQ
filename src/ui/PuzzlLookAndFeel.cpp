#include "ui/PuzzlLookAndFeel.h"

PuzzlLookAndFeel::PuzzlLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panelAlt);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, panelAlt);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, grid);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, grid);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, panel);
    setColour (juce::Slider::textBoxOutlineColourId, grid);
    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
}

void PuzzlLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float sliderPos, float start, float end, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h)).reduced (3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + sliderPos * (end - start);

    g.setColour (panelAlt);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (2.4f));

    const auto pointer = juce::Point<float> (c.x + (radius - 6.0f) * std::cos (angle - juce::MathConstants<float>::halfPi),
                                             c.y + (radius - 6.0f) * std::sin (angle - juce::MathConstants<float>::halfPi));
    g.setColour (text);
    g.fillEllipse (pointer.x - 2.5f, pointer.y - 2.5f, 5.0f, 5.0f);
}

void PuzzlLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour& background, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = background;
    if (b.getToggleState())
        c = accent;
    else if (down)
        c = background.brighter (0.15f);
    else if (highlighted)
        c = background.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? accent.brighter (0.2f) : grid);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void PuzzlLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, static_cast<float> (w), static_cast<float> (h)).reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (grid);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    const float ar = 5.0f;
    juce::Path p;
    p.addTriangle (static_cast<float> (w) - 16.0f, static_cast<float> (h) * 0.4f,
                   static_cast<float> (w) - 8.0f, static_cast<float> (h) * 0.4f,
                   static_cast<float> (w) - 12.0f, static_cast<float> (h) * 0.62f);
    g.setColour (muted);
    g.fillPath (p);
    (void) ar;
}

juce::Font PuzzlLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::FontOptions (12.0f);
}

juce::Font PuzzlLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::FontOptions (12.0f);
}

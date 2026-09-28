#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class SequencerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SequencerLookAndFeel();

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int height) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosition, float minimum, float maximum,
                          juce::Slider::SliderStyle, juce::Slider&) override;
};
}

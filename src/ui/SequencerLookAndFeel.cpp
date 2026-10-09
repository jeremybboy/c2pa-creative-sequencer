#include "SequencerLookAndFeel.h"

namespace c2paseq
{
SequencerLookAndFeel::SequencerLookAndFeel()
{
    setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(54, 59, 64));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(52, 137, 157));
    setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(228, 232, 235));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(35, 39, 43));
    setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(67, 158, 184));
    setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(115, 201, 222));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(43, 47, 51));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(83, 90, 96));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(232, 235, 237));
    setColour(juce::Label::textColourId, juce::Colour::fromRGB(226, 230, 233));
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB(43, 47, 51));
    setColour(juce::PopupMenu::textColourId, juce::Colour::fromRGB(231, 234, 236));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour::fromRGB(52, 137, 157));
}

void SequencerLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& background,
                                                bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = button.getToggleState()
        ? button.findColour(juce::TextButton::buttonOnColourId) : background;
    if (down)
        colour = colour.brighter(0.10f);
    else if (highlighted)
        colour = colour.brighter(0.06f);
    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha(0.42f);

    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour::fromRGB(108, 115, 121)
                    .withMultipliedAlpha(button.isEnabled() ? 0.75f : 0.3f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void SequencerLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool, bool)
{
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour(button.findColour(button.getToggleState()
        ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
            .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.42f));
    // Ellipsis preserves proportions; never squeeze a long plug-in name.
    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
               juce::Justification::centred, true);
}

juce::Font SequencerLookAndFeel::getTextButtonFont(juce::TextButton& button, int height)
{
    if (button.getProperties().contains("toolbarFontHeight"))
        return juce::Font(juce::FontOptions(
            static_cast<float>(button.getProperties()["toolbarFontHeight"]), juce::Font::bold));
    return juce::Font(juce::FontOptions(
        juce::jlimit(10.0f, 13.0f, static_cast<float>(height) * 0.42f),
        juce::Font::bold));
}

void SequencerLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                            int width, int height, float position,
                                            float, float,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal
        && style != juce::Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position,
                                         0.0f, 0.0f, style, slider);
        return;
    }

    const auto centreY = static_cast<float>(y + height / 2);
    const auto startX = static_cast<float>(x + 4);
    const auto endX = static_cast<float>(x + width - 4);
    g.setColour(slider.findColour(juce::Slider::backgroundColourId));
    g.drawLine(startX, centreY, endX, centreY, 5.0f);
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    g.drawLine(startX, centreY, position, centreY, 5.0f);
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillEllipse(position - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
}
}

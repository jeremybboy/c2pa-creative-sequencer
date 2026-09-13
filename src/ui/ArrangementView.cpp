#include "ArrangementView.h"

namespace c2paseq
{
ArrangementView::ArrangementView(juce::String engineStatus)
{
    title.setText("ARRANGEMENT", juce::dontSendNotification);
    title.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour::fromRGB(242, 193, 78));

    emptyState.setText("Empty arrangement\nAudio-stem import arrives in PR 004", juce::dontSendNotification);
    emptyState.setFont(juce::FontOptions(24.0f));
    emptyState.setJustificationType(juce::Justification::centred);
    emptyState.setColour(juce::Label::textColourId, juce::Colour::fromRGB(224, 218, 207));

    status.setText(std::move(engineStatus), juce::dontSendNotification);
    status.setFont(juce::FontOptions(13.0f));
    status.setColour(juce::Label::textColourId, juce::Colour::fromRGB(152, 171, 168));

    addAndMakeVisible(title);
    addAndMakeVisible(emptyState);
    addAndMakeVisible(status);
}

void ArrangementView::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour::fromRGB(25, 25, 24));
    graphics.setColour(juce::Colour::fromRGB(67, 65, 61));

    const auto content = getLocalBounds().reduced(24).withTrimmedTop(44).withTrimmedBottom(36);
    constexpr int rowHeight = 92;

    for (int y = content.getY(); y <= content.getBottom(); y += rowHeight)
        graphics.drawHorizontalLine(y, static_cast<float>(content.getX()), static_cast<float>(content.getRight()));

    graphics.setColour(juce::Colour::fromRGB(212, 75, 55));
    graphics.fillRect(content.getX(), content.getY(), 3, content.getHeight());
}

void ArrangementView::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    title.setBounds(bounds.removeFromTop(32));
    status.setBounds(bounds.removeFromBottom(28));
    emptyState.setBounds(bounds);
}
}

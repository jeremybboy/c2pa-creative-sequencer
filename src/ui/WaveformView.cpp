#include "WaveformView.h"

namespace c2paseq
{
WaveformView::WaveformView(juce::AudioFormatManager& formatManager,
                           juce::AudioThumbnailCache& thumbnailCache,
                           juce::File audioFile,
                           juce::String clipName)
    : thumbnail(256, formatManager, thumbnailCache),
      file(std::move(audioFile)),
      name(std::move(clipName))
{
    thumbnail.addChangeListener(this);
    thumbnail.setSource(new juce::FileInputSource(file));
}

void WaveformView::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat();
    graphics.setColour(juce::Colour::fromRGB(50, 75, 72));
    graphics.fillRoundedRectangle(bounds, 4.0f);
    graphics.setColour(juce::Colour::fromRGB(242, 193, 78));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    auto waveformBounds = bounds.reduced(8.0f).withTrimmedTop(18.0f);
    graphics.setColour(juce::Colour::fromRGB(224, 218, 207));
    if (thumbnail.getTotalLength() > 0.0)
        thumbnail.drawChannels(graphics, waveformBounds.toNearestInt(), 0.0,
                               thumbnail.getTotalLength(), 1.0f);
    else
        graphics.drawText("Building waveform...", waveformBounds,
                          juce::Justification::centred);

    graphics.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    graphics.drawFittedText(name, getLocalBounds().reduced(8).removeFromTop(16),
                            juce::Justification::centredLeft, 1);
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}
}

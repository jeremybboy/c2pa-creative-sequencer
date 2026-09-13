#include "TrackHeaderView.h"

namespace c2paseq
{
TrackHeaderView::TrackHeaderView(int index) : trackIndex(index)
{
    number.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    number.setJustificationType(juce::Justification::centred);
    number.setText(juce::String(index + 1), juce::dontSendNotification);
    nameEditor.setEditable(false, true, false);
    nameEditor.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    nameEditor.onTextChange = [this]
    {
        if (onNameChanged)
            onNameChanged(trackIndex, nameEditor.getText());
    };
    mute.setClickingTogglesState(true);
    solo.setClickingTogglesState(true);
    mute.onClick = [this] { if (onMuteChanged) onMuteChanged(trackIndex, mute.getToggleState()); };
    solo.onClick = [this] { if (onSoloChanged) onSoloChanged(trackIndex, solo.getToggleState()); };
    for (auto* slider : { &gain, &panControl })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    }
    gain.setRange(-60.0, 12.0, 0.1);
    gain.setTextValueSuffix(" dB");
    panControl.setRange(-1.0, 1.0, 0.01);
    gain.onDragEnd = [this] { if (onGainChanged) onGainChanged(trackIndex, gain.getValue()); };
    panControl.onDragEnd = [this] { if (onPanChanged) onPanChanged(trackIndex, panControl.getValue()); };
    addAndMakeVisible(number);
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(mute);
    addAndMakeVisible(solo);
    addAndMakeVisible(gain);
    addAndMakeVisible(panControl);
}

void TrackHeaderView::setState(const juce::String& trackName, double gainDb, double pan,
                               bool muted, bool soloed, juce::Colour colour)
{
    accent = colour;
    nameEditor.setText(trackName, juce::dontSendNotification);
    gain.setValue(gainDb, juce::dontSendNotification);
    panControl.setValue(pan, juce::dontSendNotification);
    mute.setToggleState(muted, juce::dontSendNotification);
    solo.setToggleState(soloed, juce::dontSendNotification);
    mute.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.25f));
    solo.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.25f));
    repaint();
}

void TrackHeaderView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(62, 65, 69));
    g.setColour(accent);
    g.fillRect(0, 0, 4, getHeight());
    g.setColour(juce::Colour::fromRGB(88, 92, 97));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TrackHeaderView::resized()
{
    auto area = getLocalBounds().reduced(7, 5);
    auto top = area.removeFromTop(25);
    number.setBounds(top.removeFromLeft(24));
    solo.setBounds(top.removeFromRight(27).reduced(1));
    mute.setBounds(top.removeFromRight(27).reduced(1));
    nameEditor.setBounds(top.reduced(3, 0));
    auto gainRow = area.removeFromTop(22);
    gain.setBounds(gainRow);
    panControl.setBounds(area.removeFromTop(22));
}
}

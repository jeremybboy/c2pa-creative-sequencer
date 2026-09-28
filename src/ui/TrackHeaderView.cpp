#include "TrackHeaderView.h"

namespace c2paseq
{
TrackHeaderView::TrackHeaderView(int index) : trackIndex(index)
{
    number.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    number.setJustificationType(juce::Justification::centred);
    number.setColour(juce::Label::textColourId, juce::Colour::fromRGB(179, 186, 191));
    number.setText(juce::String(index + 1), juce::dontSendNotification);
    typeBadge.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    typeBadge.setJustificationType(juce::Justification::centred);
    typeBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(222, 211, 246));
    typeBadge.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(91, 69, 126));
    nameEditor.setEditable(false, true, false);
    nameEditor.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    nameEditor.setColour(juce::Label::textColourId, juce::Colour::fromRGB(239, 241, 243));
    nameEditor.onTextChange = [this]
    {
        if (onNameChanged)
            onNameChanged(trackIndex, nameEditor.getText());
    };
    mute.setClickingTogglesState(true);
    solo.setClickingTogglesState(true);
    mute.onClick = [this] { if (onMuteChanged) onMuteChanged(trackIndex, mute.getToggleState()); };
    solo.onClick = [this] { if (onSoloChanged) onSoloChanged(trackIndex, solo.getToggleState()); };
    deleteTrack.setTooltip("Delete track");
    deleteTrack.onClick = [this] { if (onDeleteTrack) onDeleteTrack(trackIndex); };
    for (auto* slider : { &gain, &panControl })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    }
    gain.setRange(-60.0, 12.0, 0.1);
    gain.setTextValueSuffix(" dB");
    panControl.setRange(-1.0, 1.0, 0.01);
    gain.onDragStart = [this]
    {
        gainGestureActive = true;
        if (onGainGestureStart) onGainGestureStart(trackIndex);
    };
    gain.onValueChange = [this]
    {
        if (gainGestureActive)
        {
            if (onGainPreview) onGainPreview(trackIndex, gain.getValue());
        }
        else if (onGainChanged)
            onGainChanged(trackIndex, gain.getValue());
    };
    gain.onDragEnd = [this]
    {
        if (onGainGestureEnd) onGainGestureEnd(trackIndex);
        gainGestureActive = false;
    };
    panControl.onDragStart = [this]
    {
        panGestureActive = true;
        if (onPanGestureStart) onPanGestureStart(trackIndex);
    };
    panControl.onValueChange = [this]
    {
        if (panGestureActive)
        {
            if (onPanPreview) onPanPreview(trackIndex, panControl.getValue());
        }
        else if (onPanChanged)
            onPanChanged(trackIndex, panControl.getValue());
    };
    panControl.onDragEnd = [this]
    {
        if (onPanGestureEnd) onPanGestureEnd(trackIndex);
        panGestureActive = false;
    };
    pluginMenu.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Scan VST3 Plug-ins...");
        if (currentPlugin.has_value())
        {
            menu.addSeparator();
            menu.addSectionHeader(currentPlugin->missing
                ? "Missing Plugin: " + currentPlugin->name
                : currentPlugin->name);
            menu.addItem(2, "Open Plugin", ! currentPlugin->missing);
            menu.addItem(3, "Bypass", ! currentPlugin->missing,
                         currentPlugin->bypassed);
            menu.addItem(4, "Remove");
        }
        menu.addSeparator();
        menu.addSectionHeader("VST3 Audio Effects");
        auto itemId = 1000;
        auto effectCount = 0;
        for (const auto& plugin : availablePlugins)
        {
            if (plugin.isInstrument)
                continue;
            menu.addItem(itemId++, plugin.vendor.isNotEmpty()
                ? plugin.vendor + " — " + plugin.name : plugin.name);
            ++effectCount;
        }
        if (effectCount == 0)
            menu.addItem(999, "No scanned audio effects", false);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pluginMenu),
            [safe = juce::Component::SafePointer<TrackHeaderView>(this)](int result)
            {
                if (safe == nullptr || result == 0)
                    return;
                if (result == 1 && safe->onScanPlugins) safe->onScanPlugins();
                else if (result == 2 && safe->onOpenPlugin) safe->onOpenPlugin(safe->trackIndex);
                else if (result == 3 && safe->onBypassPlugin && safe->currentPlugin)
                    safe->onBypassPlugin(safe->trackIndex, ! safe->currentPlugin->bypassed);
                else if (result == 4 && safe->onRemovePlugin) safe->onRemovePlugin(safe->trackIndex);
                else if (result >= 1000 && safe->onLoadPlugin)
                {
                    auto selected = result - 1000;
                    for (const auto& plugin : safe->availablePlugins)
                    {
                        if (plugin.isInstrument)
                            continue;
                        if (selected-- == 0)
                        {
                            safe->onLoadPlugin(safe->trackIndex, plugin.identifier);
                            break;
                        }
                    }
                }
            });
    };
    addAndMakeVisible(number);
    addAndMakeVisible(typeBadge);
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(mute);
    addAndMakeVisible(solo);
    addAndMakeVisible(pluginMenu);
    addAndMakeVisible(deleteTrack);
    addAndMakeVisible(gain);
    addAndMakeVisible(panControl);
}

void TrackHeaderView::setPluginState(const std::optional<TrackPluginSnapshot>& plugin,
                                     const std::vector<PluginDescriptor>& plugins)
{
    currentPlugin = plugin;
    availablePlugins = plugins;
    if (currentType == TrackType::midi)
    {
        pluginMenu.setButtonText("MIDI");
        pluginMenu.setTooltip("Instrument hosting is not part of PR 018");
        return;
    }
    if (! currentPlugin.has_value())
    {
        pluginMenu.setButtonText("VST3");
        pluginMenu.setTooltip("Scan, load, and manage one VST3 audio effect");
        return;
    }
    pluginMenu.setButtonText(currentPlugin->missing ? "Missing" : currentPlugin->name);
    pluginMenu.setTooltip(currentPlugin->missing
        ? "Missing Plugin: " + currentPlugin->name
        : currentPlugin->vendor + " — " + currentPlugin->name);
}

void TrackHeaderView::setState(const juce::String& trackName, double gainDb, double pan,
                               bool muted, bool soloed, TrackType type, juce::Colour colour)
{
    accent = colour;
    currentType = type;
    const auto audioControlsEnabled = currentType == TrackType::audio;
    nameEditor.setText(trackName, juce::dontSendNotification);
    gain.setValue(gainDb, juce::dontSendNotification);
    panControl.setValue(pan, juce::dontSendNotification);
    mute.setToggleState(muted, juce::dontSendNotification);
    solo.setToggleState(soloed, juce::dontSendNotification);
    typeBadge.setText(currentType == TrackType::midi ? "MIDI" : "",
                      juce::dontSendNotification);
    typeBadge.setVisible(currentType == TrackType::midi);
    for (auto* component : { static_cast<juce::Component*>(&mute),
                             static_cast<juce::Component*>(&solo),
                             static_cast<juce::Component*>(&pluginMenu),
                             static_cast<juce::Component*>(&gain),
                             static_cast<juce::Component*>(&panControl) })
    {
        component->setEnabled(audioControlsEnabled);
        component->setVisible(audioControlsEnabled);
    }
    mute.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.25f));
    solo.setColour(juce::TextButton::buttonOnColourId, accent.darker(0.25f));
    resized();
    repaint();
}

void TrackHeaderView::setMeterPeak(TrackLevelSnapshot peak, bool audible)
{
    if (currentType != TrackType::audio)
        return;
    const auto ignored = meterBallistics.update(peak, audible);
    juce::ignoreUnused(ignored);
    repaint(meterBounds.expanded(1));
}

void TrackHeaderView::paint(juce::Graphics& g)
{
    g.fillAll(currentType == TrackType::midi ? juce::Colour::fromRGB(56, 53, 63)
                                             : juce::Colour::fromRGB(53, 57, 61));
    g.setColour(juce::Colour::fromRGB(47, 51, 55));
    g.fillRect(4, 0, getWidth() - 4, 34);
    g.setColour(accent);
    g.fillRect(0, 0, 4, getHeight());
    g.setColour(juce::Colour::fromRGB(75, 80, 85));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    if (currentType != TrackType::audio)
        return;

    g.setColour(juce::Colour::fromRGB(31, 33, 35));
    g.fillRoundedRectangle(meterBounds.toFloat(), 2.0f);
    const auto levels = meterBallistics.current();
    const auto drawChannel = [&g](juce::Rectangle<int> channel, float level)
    {
        const auto height = juce::roundToInt(level * static_cast<float>(channel.getHeight()));
        if (height <= 0)
            return;
        auto fill = channel.removeFromBottom(height).toFloat();
        g.setColour(level > 0.92f ? juce::Colour::fromRGB(238, 96, 72)
                                  : level > 0.74f ? juce::Colour::fromRGB(232, 190, 71)
                                                  : juce::Colour::fromRGB(71, 190, 137));
        g.fillRoundedRectangle(fill, 1.0f);
    };
    auto channels = meterBounds.reduced(2);
    drawChannel(channels.removeFromLeft(3), levels.left);
    channels.removeFromLeft(1);
    drawChannel(channels.removeFromLeft(3), levels.right);
}

void TrackHeaderView::resized()
{
    auto area = getLocalBounds().reduced(7, 5);
    if (currentType == TrackType::audio)
    {
        meterBounds = area.removeFromRight(11).reduced(1, 2);
        area.removeFromRight(4);
    }
    else
    {
        meterBounds = {};
    }
    auto top = area.removeFromTop(25);
    number.setBounds(top.removeFromLeft(24));
    typeBadge.setBounds(top.removeFromLeft(currentType == TrackType::midi ? 36 : 0).reduced(2, 3));
    deleteTrack.setBounds(top.removeFromRight(24).reduced(1));
    if (currentType == TrackType::audio)
    {
        solo.setBounds(top.removeFromRight(27).reduced(1));
        mute.setBounds(top.removeFromRight(27).reduced(1));
        pluginMenu.setBounds(top.removeFromRight(58).reduced(1));
    }
    else
    {
        solo.setBounds({});
        mute.setBounds({});
        pluginMenu.setBounds({});
    }
    nameEditor.setBounds(top.reduced(3, 0));
    if (currentType == TrackType::audio)
    {
        auto gainRow = area.removeFromTop(22);
        gain.setBounds(gainRow);
        panControl.setBounds(area.removeFromTop(22));
    }
    else
    {
        gain.setBounds({});
        panControl.setBounds({});
    }
}
}

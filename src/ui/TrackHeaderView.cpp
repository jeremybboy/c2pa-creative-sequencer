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
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(mute);
    addAndMakeVisible(solo);
    addAndMakeVisible(pluginMenu);
    addAndMakeVisible(gain);
    addAndMakeVisible(panControl);
}

void TrackHeaderView::setPluginState(const std::optional<TrackPluginSnapshot>& plugin,
                                     const std::vector<PluginDescriptor>& plugins)
{
    currentPlugin = plugin;
    availablePlugins = plugins;
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
    pluginMenu.setBounds(top.removeFromRight(62).reduced(1));
    nameEditor.setBounds(top.reduced(3, 0));
    auto gainRow = area.removeFromTop(22);
    gain.setBounds(gainRow);
    panControl.setBounds(area.removeFromTop(22));
}
}

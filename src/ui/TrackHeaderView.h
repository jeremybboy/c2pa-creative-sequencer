#pragma once

#include "engine/ProjectEngine.h"
#include "plugins/PluginDescriptor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class TrackHeaderView final : public juce::Component
{
public:
    explicit TrackHeaderView(int index);
    void setState(const juce::String& name, double gainDb, double pan,
                  bool muted, bool soloed, juce::Colour colour);
    void setPluginState(const std::optional<TrackPluginSnapshot>&,
                        const std::vector<PluginDescriptor>&);
    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void(int, juce::String)> onNameChanged;
    std::function<void(int, bool)> onMuteChanged;
    std::function<void(int, bool)> onSoloChanged;
    std::function<void(int, double)> onGainChanged;
    std::function<void(int, double)> onPanChanged;
    std::function<void()> onScanPlugins;
    std::function<void(int, juce::String)> onLoadPlugin;
    std::function<void(int)> onOpenPlugin;
    std::function<void(int, bool)> onBypassPlugin;
    std::function<void(int)> onRemovePlugin;

private:
    int trackIndex;
    juce::Colour accent;
    juce::Label number;
    juce::Label nameEditor;
    juce::TextButton mute { "M" };
    juce::TextButton solo { "S" };
    juce::TextButton pluginMenu { "VST3" };
    juce::Slider gain;
    juce::Slider panControl;
    std::optional<TrackPluginSnapshot> currentPlugin;
    std::vector<PluginDescriptor> availablePlugins;
};
}

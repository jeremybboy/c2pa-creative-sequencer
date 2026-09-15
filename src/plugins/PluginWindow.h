#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace c2paseq
{
class PluginWindow final : public juce::DocumentWindow
{
public:
    explicit PluginWindow(juce::AudioPluginInstance&, std::function<void()> closeRequested,
                          bool showWindow = true);
    void closeButtonPressed() override;

private:
    juce::AudioPluginInstance& plugin;
    std::function<void()> closeRequested;
};
}

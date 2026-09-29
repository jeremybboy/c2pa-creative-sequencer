#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace c2paseq
{
class PluginWindow final : public juce::DocumentWindow,
                           private juce::KeyListener
{
public:
    explicit PluginWindow(juce::AudioPluginInstance&, std::function<void()> closeRequested,
                          std::function<bool(const juce::KeyPress&)> keyHandler = {},
                          bool showWindow = true);
    void closeButtonPressed() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    void listenForKeysFrom(juce::Component&);

    juce::AudioPluginInstance& plugin;
    std::function<void()> closeRequested;
    std::function<bool(const juce::KeyPress&)> keyHandler;
};
}

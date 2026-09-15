#include "PluginWindow.h"

namespace c2paseq
{
PluginWindow::PluginWindow(juce::AudioPluginInstance& instance,
                           std::function<void()> onCloseRequested,
                           bool showWindow)
    : juce::DocumentWindow(instance.getName(), juce::Colours::black,
                           juce::DocumentWindow::closeButton),
      plugin(instance), closeRequested(std::move(onCloseRequested))
{
    auto editor = plugin.createEditorIfNeeded();
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor(plugin);
    setContentOwned(editor, true);
    if (showWindow)
    {
        setUsingNativeTitleBar(true);
        setResizable(editor->isResizable(), false);
        centreWithSize(std::max(320, editor->getWidth()), std::max(180, editor->getHeight()));
        setVisible(true);
        toFront(true);
    }
}

void PluginWindow::closeButtonPressed()
{
    setVisible(false);
    auto callback = std::move(closeRequested);
    if (callback)
        callback();
}
}

#include "PluginWindow.h"

namespace c2paseq
{
PluginWindow::PluginWindow(juce::AudioPluginInstance& instance)
    : juce::DocumentWindow(instance.getName(), juce::Colours::black,
                           juce::DocumentWindow::closeButton),
      plugin(instance)
{
    auto editor = plugin.createEditorIfNeeded();
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor(plugin);
    setUsingNativeTitleBar(true);
    setContentOwned(editor, true);
    setResizable(editor->isResizable(), false);
    centreWithSize(std::max(320, editor->getWidth()), std::max(180, editor->getHeight()));
    setVisible(true);
    toFront(true);
}

void PluginWindow::closeButtonPressed()
{
    setVisible(false);
}
}

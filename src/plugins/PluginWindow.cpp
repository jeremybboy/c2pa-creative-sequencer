#include "PluginWindow.h"

namespace c2paseq
{
PluginWindow::PluginWindow(juce::AudioPluginInstance& instance,
                           std::function<void()> onCloseRequested,
                           std::function<bool(const juce::KeyPress&)> onKey,
                           bool showWindow)
    : juce::DocumentWindow(instance.getName(), juce::Colours::black,
                           juce::DocumentWindow::closeButton),
      plugin(instance), closeRequested(std::move(onCloseRequested)),
      keyHandler(std::move(onKey))
{
    auto editor = plugin.createEditorIfNeeded();
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor(plugin);
    setContentOwned(editor, true);
    listenForKeysFrom(*editor);
    if (showWindow)
    {
        setUsingNativeTitleBar(true);
        setResizable(editor->isResizable(), false);
        centreWithSize(std::max(320, editor->getWidth()), std::max(180, editor->getHeight()));
        setVisible(true);
        toFront(true);
    }
}

bool PluginWindow::keyPressed(const juce::KeyPress& key)
{
    return keyHandler && keyHandler(key);
}

bool PluginWindow::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return keyHandler && keyHandler(key);
}

void PluginWindow::listenForKeysFrom(juce::Component& component)
{
    component.addKeyListener(this);
    for (auto* child : component.getChildren())
        listenForKeysFrom(*child);
}

void PluginWindow::closeButtonPressed()
{
    setVisible(false);
    auto callback = std::move(closeRequested);
    if (callback)
        callback();
}
}

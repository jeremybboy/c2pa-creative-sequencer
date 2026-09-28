#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class IconButton final : public juce::Button
{
public:
    enum class Icon
    {
        play,
        pause,
        stop,
        loop,
        undo,
        redo,
        zoomIn,
        zoomOut,
        audio,
        close
    };

    IconButton(juce::String name, Icon);
    void setIcon(Icon);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    static void drawIcon(juce::Graphics&, juce::Rectangle<float>, Icon,
                         juce::Colour);

    Icon icon;
};
}

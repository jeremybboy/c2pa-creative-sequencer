#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class AudioEngine;

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow(AudioEngine& audioEngine);
    void closeButtonPressed() override;
};
}

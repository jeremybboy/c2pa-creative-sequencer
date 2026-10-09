#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace c2paseq
{
class AudioEngine;
class ArrangementView;

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow(AudioEngine& audioEngine,
                        std::function<void()> audioWMarkSetupRequest = {});
    void closeButtonPressed() override;
    [[nodiscard]] bool canQuitDuringOfflineExport();
    void audioWMarkSetupFinished(bool installed, const juce::String& message);

private:
    ArrangementView* arrangementView = nullptr;
};
}

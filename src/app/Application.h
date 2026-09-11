#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace c2paseq
{
class AudioEngine;
class MainWindow;

class Application final : public juce::JUCEApplication
{
public:
    Application() = default;
    ~Application() override;

    [[nodiscard]] const juce::String getApplicationName() override;
    [[nodiscard]] const juce::String getApplicationVersion() override;
    [[nodiscard]] bool moreThanOneInstanceAllowed() override;

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted(const juce::String& commandLine) override;

private:
    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<MainWindow> mainWindow;
};
}

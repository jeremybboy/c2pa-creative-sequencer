#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace c2paseq
{
class AudioEngine;
class DemoRuntimeController;
class MainWindow;

class Application final : public juce::JUCEApplication,
                          public juce::MenuBarModel
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

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                    const juce::String& menuName) override;
    void menuItemSelected(int menuItemId, int topLevelMenuIndex) override;

private:
    void requestAudioWMarkInstall(bool enableAfterInstall,
                                  bool launchDemoAfterInstall = false);
    void launchRecoveryDemo();
    void reportRuntimeResult(const juce::String& title, const juce::Result& result);

    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<DemoRuntimeController> demoRuntime;
    std::unique_ptr<MainWindow> mainWindow;
};
}

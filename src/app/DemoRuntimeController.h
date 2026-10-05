#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

namespace c2paseq
{
struct DemoRuntimePaths
{
    juce::File setupScript;
    juce::File serverScript;
    juce::File audioWMarkExecutable;

    [[nodiscard]] static DemoRuntimePaths fromExecutable(const juce::File& executable);
};

class DemoRuntimeController final
{
public:
    using Completion = std::function<void(juce::Result)>;

    explicit DemoRuntimeController(DemoRuntimePaths paths =
        DemoRuntimePaths::fromExecutable(juce::File::getSpecialLocation(
            juce::File::currentExecutableFile)));
    ~DemoRuntimeController();

    [[nodiscard]] bool audioWMarkAvailable() const;
    [[nodiscard]] bool installationInProgress() const noexcept;
    [[nodiscard]] bool recoveryDemoStarting() const noexcept;
    [[nodiscard]] bool recoveryDemoRunning() const;
    [[nodiscard]] juce::String recoveryDemoUrl() const;

    [[nodiscard]] juce::Result installAudioWMark(Completion completion);
    [[nodiscard]] juce::Result launchRecoveryDemo(Completion completion);
    void stopRecoveryDemo();

private:
    DemoRuntimePaths paths;
    juce::ChildProcess installationProcess;
    juce::ChildProcess recoveryProcess;
    std::thread installationThread;
    std::thread recoveryThread;
    mutable std::mutex recoveryProcessMutex;
    std::atomic_bool installing { false };
    std::atomic_bool startingRecovery { false };
    std::atomic_bool recoveryReady { false };
    std::atomic_bool stopRecoveryRequested { false };
    std::atomic_bool shuttingDown { false };
    std::atomic_int recoveryPort { 8787 };
};
}

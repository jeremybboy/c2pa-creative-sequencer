#include "Application.h"

#include "AppInfo.h"
#include "DemoRuntimeController.h"
#include "MainWindow.h"
#include "engine/AudioEngine.h"

namespace c2paseq
{
Application::~Application() = default;

const juce::String Application::getApplicationName()
{
    return appInfo::name.data();
}

const juce::String Application::getApplicationVersion()
{
    return appInfo::version.data();
}

bool Application::moreThanOneInstanceAllowed()
{
    return false;
}

void Application::initialise(const juce::String&)
{
    audioEngine = std::make_unique<AudioEngine>();
    demoRuntime = std::make_unique<DemoRuntimeController>();
    mainWindow = std::make_unique<MainWindow>(*audioEngine,
        [this] { requestAudioWMarkInstall(true); });
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
#endif
}

void Application::shutdown()
{
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
    mainWindow.reset();
    demoRuntime.reset();
    audioEngine.reset();
}

void Application::systemRequestedQuit()
{
    quit();
}

void Application::anotherInstanceStarted(const juce::String&)
{
    if (mainWindow != nullptr)
        mainWindow->toFront(true);
}

juce::StringArray Application::getMenuBarNames()
{
    return { "Tools" };
}

juce::PopupMenu Application::getMenuForIndex(int, const juce::String&)
{
    enum { installAudioWMark = 1, launchDemo, stopDemo };
    juce::PopupMenu menu;
    const auto installed = demoRuntime != nullptr && demoRuntime->audioWMarkAvailable();
    const auto installing = demoRuntime != nullptr && demoRuntime->installationInProgress();
    const auto startingDemo = demoRuntime != nullptr && demoRuntime->recoveryDemoStarting();
    const auto running = demoRuntime != nullptr && demoRuntime->recoveryDemoRunning();
    menu.addItem(installAudioWMark,
                 installed ? "AudioWMark Installed" :
                    (installing ? "Installing AudioWMark…" : "Install AudioWMark…"),
                 ! installed && ! installing);
    menu.addSeparator();
    menu.addItem(launchDemo,
                 startingDemo ? "Starting Recovery Demo..." : "Launch Recovery Demo",
                 ! installing && ! startingDemo);
    menu.addItem(stopDemo, "Stop Recovery Demo", running || startingDemo);
    return menu;
}

void Application::menuItemSelected(int menuItemId, int)
{
    enum { installAudioWMark = 1, launchDemo, stopDemo };
    if (menuItemId == installAudioWMark)
        requestAudioWMarkInstall(false);
    else if (menuItemId == launchDemo)
        launchRecoveryDemo();
    else if (menuItemId == stopDemo && demoRuntime != nullptr)
    {
        demoRuntime->stopRecoveryDemo();
        menuItemsChanged();
    }
}

void Application::requestAudioWMarkInstall(bool enableAfterInstall,
                                           bool launchDemoAfterInstall)
{
    if (demoRuntime == nullptr)
        return;
    if (demoRuntime->audioWMarkAvailable())
    {
        if (enableAfterInstall && mainWindow != nullptr)
            mainWindow->audioWMarkSetupFinished(true, "Audio soft binding enabled");
        if (launchDemoAfterInstall)
            launchRecoveryDemo();
        return;
    }

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon, "Install AudioWMark?",
        "This one-time setup downloads and builds pinned AudioWMark 0.6.5 and zita-resampler "
        "outside the app bundle. It requires internet access and local build tools.\n\n"
        "AudioWMark is GPL-3.0-or-later and remains an external executable.",
        "Install", "Cancel", mainWindow.get(),
        juce::ModalCallbackFunction::create(
            [this, enableAfterInstall, launchDemoAfterInstall](int result)
            {
                if (result == 0 || demoRuntime == nullptr)
                {
                    if (enableAfterInstall && mainWindow != nullptr)
                        mainWindow->audioWMarkSetupFinished(false,
                            "AudioWMark installation cancelled");
                    return;
                }
                if (mainWindow != nullptr)
                    mainWindow->audioWMarkSetupFinished(false,
                        "Installing AudioWMark in the background…");
                const auto started = demoRuntime->installAudioWMark(
                    [this, enableAfterInstall, launchDemoAfterInstall](juce::Result installation)
                    {
                        menuItemsChanged();
                        if (mainWindow != nullptr)
                            mainWindow->audioWMarkSetupFinished(
                                installation.wasOk() && enableAfterInstall,
                                installation.wasOk()
                                    ? (enableAfterInstall ? "Audio soft binding enabled"
                                                          : "AudioWMark installation complete")
                                    : installation.getErrorMessage());
                        reportRuntimeResult("AudioWMark Installation", installation);
                        if (installation.wasOk() && launchDemoAfterInstall)
                            launchRecoveryDemo();
                    });
                menuItemsChanged();
                if (started.failed())
                {
                    if (mainWindow != nullptr)
                        mainWindow->audioWMarkSetupFinished(false,
                            started.getErrorMessage());
                    reportRuntimeResult("AudioWMark Installation", started);
                }
            }));
}

void Application::launchRecoveryDemo()
{
    if (demoRuntime == nullptr)
        return;
    if (! demoRuntime->audioWMarkAvailable())
    {
        requestAudioWMarkInstall(false, true);
        return;
    }
    const auto result = demoRuntime->launchRecoveryDemo(
        [this](juce::Result startup)
        {
            menuItemsChanged();
            if (startup.failed())
            {
                reportRuntimeResult("Recovery Demo", startup);
                return;
            }
            if (demoRuntime == nullptr
                || ! juce::URL(demoRuntime->recoveryDemoUrl()).launchInDefaultBrowser())
                reportRuntimeResult("Recovery Demo", juce::Result::fail(
                    "The recovery demo started, but the browser could not be opened"));
        });
    menuItemsChanged();
    if (result.failed())
        reportRuntimeResult("Recovery Demo", result);
}

void Application::reportRuntimeResult(const juce::String& title, const juce::Result& result)
{
    juce::AlertWindow::showMessageBoxAsync(
        result.wasOk() ? juce::MessageBoxIconType::InfoIcon
                       : juce::MessageBoxIconType::WarningIcon,
        title, result.wasOk() ? "Completed successfully" : result.getErrorMessage(),
        "OK", mainWindow.get());
}
}

START_JUCE_APPLICATION(c2paseq::Application)

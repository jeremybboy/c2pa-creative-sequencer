#include "MainWindow.h"

#include "AppInfo.h"
#include "engine/AudioEngine.h"
#include "ui/ArrangementView.h"

namespace c2paseq
{
MainWindow::MainWindow(AudioEngine& audioEngine,
                       std::function<void()> audioWMarkSetupRequest)
    : DocumentWindow(appInfo::name.data(),
                     juce::Colour::fromRGB(25, 25, 24),
                     DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(1040, 600, 3840, 2160);
    arrangementView = new ArrangementView(audioEngine, std::move(audioWMarkSetupRequest));
    setContentOwned(arrangementView, true);
    centreWithSize(appInfo::defaultWindowWidth, appInfo::defaultWindowHeight);
    setVisible(true);
}

void MainWindow::audioWMarkSetupFinished(bool installed, const juce::String& message)
{
    if (arrangementView != nullptr)
        arrangementView->audioWMarkSetupFinished(installed, message);
}

void MainWindow::closeButtonPressed()
{
    if (auto* application = juce::JUCEApplication::getInstance())
        application->systemRequestedQuit();
}

bool MainWindow::canQuitDuringOfflineExport()
{
    if (arrangementView == nullptr || ! arrangementView->offlineExportActive())
        return true;
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
        "Operation in Progress", "Stop or cancel the recording first; wait for signing, export or bounce to finish before quitting.",
        "OK", this);
    return false;
}
}

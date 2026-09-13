#include "MainWindow.h"

#include "AppInfo.h"
#include "engine/AudioEngine.h"
#include "ui/ArrangementView.h"

namespace c2paseq
{
MainWindow::MainWindow(AudioEngine& audioEngine)
    : DocumentWindow(appInfo::name.data(),
                     juce::Colour::fromRGB(25, 25, 24),
                     DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(760, 480, 3840, 2160);
    setContentOwned(new ArrangementView(audioEngine), true);
    centreWithSize(appInfo::defaultWindowWidth, appInfo::defaultWindowHeight);
    setVisible(true);
}

void MainWindow::closeButtonPressed()
{
    if (auto* application = juce::JUCEApplication::getInstance())
        application->systemRequestedQuit();
}
}

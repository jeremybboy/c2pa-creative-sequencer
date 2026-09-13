#include "Application.h"

#include "AppInfo.h"
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
    mainWindow = std::make_unique<MainWindow>(*audioEngine);
}

void Application::shutdown()
{
    mainWindow.reset();
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
}

START_JUCE_APPLICATION(c2paseq::Application)

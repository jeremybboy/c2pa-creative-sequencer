#include "AudioEngine.h"

namespace c2paseq
{
AudioEngine::AudioEngine()
    : projectEngine(tracktion)
{
}

bool AudioEngine::isInitialised() const noexcept
{
    return tracktion.isInitialised();
}

juce::String AudioEngine::status() const
{
    return tracktion.audioDeviceDescription();
}

AudioDeviceSnapshot AudioEngine::audioDeviceSnapshot() const
{
    return tracktion.audioDeviceSnapshot();
}

TransportSnapshot AudioEngine::transportSnapshot() const
{
    return tracktion.transportSnapshot();
}

juce::AudioDeviceManager& AudioEngine::audioDeviceManager() noexcept
{
    return tracktion.audioDeviceManager();
}

void AudioEngine::play()
{
    tracktion.play();
}

void AudioEngine::pause()
{
    tracktion.pause();
}

void AudioEngine::stop()
{
    tracktion.stop();
}

void AudioEngine::seek(double positionSeconds)
{
    tracktion.seek(positionSeconds);
}

void AudioEngine::setLooping(bool shouldLoop)
{
    tracktion.setLooping(shouldLoop);
}

void AudioEngine::setBpm(double bpm)
{
    tracktion.setBpm(bpm);
    projectEngine.setBpm(bpm);
}

juce::Result AudioEngine::createProject(const juce::File& projectFolder,
                                        const juce::String& projectName)
{
    return projectEngine.createProject(projectFolder, projectName);
}

juce::Result AudioEngine::saveProject()
{
    return projectEngine.saveProject();
}

juce::Result AudioEngine::openProject(const juce::File& projectFolder)
{
    return projectEngine.openProject(projectFolder);
}

bool AudioEngine::hasProject() const noexcept
{
    return projectEngine.hasProject();
}

juce::String AudioEngine::projectName() const
{
    return projectEngine.displayName();
}
}

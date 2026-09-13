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

juce::Result AudioEngine::importAudio(const juce::File& source,
                                      double startSeconds)
{
    return projectEngine.importAudio(source, startSeconds);
}

bool AudioEngine::isSupportedAudioFile(const juce::File& file)
{
    return file.hasFileExtension("wav;aif;aiff;mp3");
}

std::vector<ArrangementTrackSnapshot> AudioEngine::arrangementSnapshot() const
{
    return projectEngine.arrangementSnapshot();
}

juce::AudioFormatManager& AudioEngine::audioFormatManager() noexcept
{
    return tracktion.audioFormatManager();
}

juce::AudioThumbnailCache& AudioEngine::audioThumbnailCache() noexcept
{
    return tracktion.audioThumbnailCache();
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

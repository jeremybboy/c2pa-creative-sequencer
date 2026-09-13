#pragma once

#include "TracktionAdapter.h"

namespace c2paseq
{
class AudioEngine final
{
public:
    AudioEngine() = default;

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String status() const;
    [[nodiscard]] AudioDeviceSnapshot audioDeviceSnapshot() const;
    [[nodiscard]] TransportSnapshot transportSnapshot() const;
    [[nodiscard]] juce::AudioDeviceManager& audioDeviceManager() noexcept;

    void play();
    void pause();
    void stop();
    void seek(double positionSeconds);
    void setLooping(bool shouldLoop);
    void setBpm(double bpm);

private:
    TracktionAdapter tracktion;
};
}

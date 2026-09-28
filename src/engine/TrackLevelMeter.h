#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace c2paseq
{
struct TrackLevelSnapshot
{
    float left = 0.0f;
    float right = 0.0f;
};

class TrackLevelAccumulator final
{
public:
    void process(const juce::AudioBuffer<float>&, int startSample, int numSamples) noexcept;
    [[nodiscard]] TrackLevelSnapshot consume() noexcept;
    void reset() noexcept;

private:
    static void storeMaximum(std::atomic<float>&, float) noexcept;

    static_assert(std::atomic<float>::is_always_lock_free,
                  "Track meters require lock-free float atomics");
    std::atomic<float> left { 0.0f };
    std::atomic<float> right { 0.0f };
};

class TrackLevelBallistics final
{
public:
    [[nodiscard]] TrackLevelSnapshot update(TrackLevelSnapshot peak,
                                            bool audible) noexcept;
    [[nodiscard]] TrackLevelSnapshot current() const noexcept;
    void reset() noexcept;

private:
    static float normalise(float linearGain) noexcept;
    static float smooth(float current, float target) noexcept;

    TrackLevelSnapshot displayed;
};
}

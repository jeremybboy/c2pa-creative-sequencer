#include "TrackLevelMeter.h"

#include <algorithm>
#include <cmath>

namespace c2paseq
{
void TrackLevelAccumulator::process(const juce::AudioBuffer<float>& buffer,
                                    int startSample, int numSamples) noexcept
{
    if (buffer.getNumChannels() == 0 || numSamples <= 0)
        return;

    const auto safeStart = juce::jlimit(0, buffer.getNumSamples(), startSample);
    const auto safeCount = juce::jlimit(0, buffer.getNumSamples() - safeStart, numSamples);
    if (safeCount == 0)
        return;

    const auto leftPeak = buffer.getMagnitude(0, safeStart, safeCount);
    const auto rightPeak = buffer.getNumChannels() > 1
        ? buffer.getMagnitude(1, safeStart, safeCount) : leftPeak;
    storeMaximum(left, leftPeak);
    storeMaximum(right, rightPeak);
}

TrackLevelSnapshot TrackLevelAccumulator::consume() noexcept
{
    return { left.exchange(0.0f, std::memory_order_relaxed),
             right.exchange(0.0f, std::memory_order_relaxed) };
}

void TrackLevelAccumulator::reset() noexcept
{
    left.store(0.0f, std::memory_order_relaxed);
    right.store(0.0f, std::memory_order_relaxed);
}

void TrackLevelAccumulator::storeMaximum(std::atomic<float>& destination,
                                         float value) noexcept
{
    auto current = destination.load(std::memory_order_relaxed);
    while (current < value
           && ! destination.compare_exchange_weak(current, value,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed))
    {
    }
}

TrackLevelSnapshot TrackLevelBallistics::update(TrackLevelSnapshot peak,
                                                bool audible) noexcept
{
    const auto targetLeft = audible ? normalise(peak.left) : 0.0f;
    const auto targetRight = audible ? normalise(peak.right) : 0.0f;
    displayed.left = smooth(displayed.left, targetLeft);
    displayed.right = smooth(displayed.right, targetRight);
    return displayed;
}

TrackLevelSnapshot TrackLevelBallistics::current() const noexcept
{
    return displayed;
}

void TrackLevelBallistics::reset() noexcept
{
    displayed = {};
}

float TrackLevelBallistics::normalise(float linearGain) noexcept
{
    if (! std::isfinite(linearGain) || linearGain <= 0.0f)
        return 0.0f;

    constexpr auto floorDb = -60.0f;
    const auto db = juce::Decibels::gainToDecibels(linearGain, floorDb);
    return juce::jlimit(0.0f, 1.0f, (db - floorDb) / -floorDb);
}

float TrackLevelBallistics::smooth(float current, float target) noexcept
{
    constexpr auto release = 0.86f;
    return target >= current ? target : std::max(target, current * release);
}
}

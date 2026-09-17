#pragma once

#include "SoftBindingPayload.h"

#include <functional>

namespace c2paseq
{
struct WatermarkEmbedResult
{
    double elapsedSeconds = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    juce::int64 frames = 0;
};

class WatermarkService
{
public:
    virtual ~WatermarkService() = default;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual juce::String statusDescription() const = 0;
    [[nodiscard]] virtual juce::Result embed(const juce::File& input,
                                              const juce::File& output,
                                              const SoftBindingPayload&,
                                              WatermarkEmbedResult&,
                                              const std::function<bool()>& shouldCancel = {}) = 0;
};
}

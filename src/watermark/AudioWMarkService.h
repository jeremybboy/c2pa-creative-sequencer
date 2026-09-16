#pragma once

#include "WatermarkService.h"

namespace c2paseq
{
class AudioWMarkService final : public WatermarkService
{
public:
    explicit AudioWMarkService(juce::File executable = defaultExecutable());

    [[nodiscard]] static juce::File defaultExecutable();
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] juce::String statusDescription() const override;
    [[nodiscard]] juce::Result embed(const juce::File& input,
                                     const juce::File& output,
                                     const SoftBindingPayload& payload,
                                     WatermarkEmbedResult& details,
                                     const std::function<bool()>& shouldCancel = {}) override;

private:
    juce::File executable;
};
}

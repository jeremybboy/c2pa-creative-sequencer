#pragma once

#include "WatermarkService.h"

namespace c2paseq
{
class WavMarkService final : public WatermarkService
{
public:
    explicit WavMarkService(juce::File runtimeDirectory = defaultRuntimeDirectory());

    [[nodiscard]] static juce::File defaultRuntimeDirectory();
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] juce::String statusDescription() const override;
    [[nodiscard]] juce::Result embed(const juce::File&, const juce::File&,
                                      const SoftBindingPayload&,
                                      WatermarkEmbedResult&) override;
    [[nodiscard]] juce::Result decode(const juce::File&, SoftBindingPayload&) override;

private:
    [[nodiscard]] juce::Result run(const juce::StringArray&, juce::var&) const;
    [[nodiscard]] juce::File pythonExecutable() const;
    [[nodiscard]] juce::File helperScript() const;

    juce::File runtime;
};
}

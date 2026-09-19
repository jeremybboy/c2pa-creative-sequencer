#pragma once

#include "FingerprintService.h"

namespace c2paseq
{
class AudfprintService final : public FingerprintService
{
public:
    explicit AudfprintService(juce::File executable = defaultExecutable());

    [[nodiscard]] static juce::File defaultExecutable();
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] juce::String statusDescription() const override;
    [[nodiscard]] juce::Result compute(const juce::File&, const juce::File&,
                                       FingerprintRegistration&,
                                       const std::function<bool()>& = {}) override;

private:
    juce::File executable;
};
}

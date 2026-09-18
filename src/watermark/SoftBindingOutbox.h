#pragma once

#include "SoftBindingPayload.h"
#include "fingerprint/FingerprintService.h"

#include <functional>
#include <vector>

namespace c2paseq
{
struct SoftBindingPublication
{
    SoftBindingPayload payload;
    juce::String activeManifest;
    juce::File packageDirectory;
};

struct SoftBindingPublicationRequest
{
    std::optional<SoftBindingPayload> watermark;
    std::optional<FingerprintRegistration> fingerprint;
    std::vector<std::uint8_t> manifestBytes;
    juce::String title;
    juce::String claimGenerator;
    juce::String activeManifest;
};

class SoftBindingOutbox final
{
public:
    using PayloadGenerator = std::function<SoftBindingPayload()>;

    explicit SoftBindingOutbox(juce::File directory = defaultDirectory(),
                               PayloadGenerator generator = {});

    [[nodiscard]] static juce::File defaultDirectory();
    [[nodiscard]] juce::Result allocatePayload(SoftBindingPayload&) const;
    [[nodiscard]] juce::Result publish(const SoftBindingPayload&,
                                       const std::vector<std::uint8_t>& manifestBytes,
                                       const juce::String& title,
                                       const juce::String& claimGenerator,
                                       const juce::String& activeManifest,
                                       SoftBindingPublication&);
    [[nodiscard]] juce::Result publish(const SoftBindingPublicationRequest&,
                                       SoftBindingPublication&);
    [[nodiscard]] const juce::File& directory() const noexcept { return root; }

private:
    [[nodiscard]] SoftBindingPayload randomPayload() const;

    juce::File root;
    PayloadGenerator payloadGenerator;
};
}

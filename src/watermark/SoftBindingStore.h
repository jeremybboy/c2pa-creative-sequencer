#pragma once

#include "SoftBindingPayload.h"

#include <functional>
#include <vector>

namespace c2paseq
{
struct StoredSoftBinding
{
    SoftBindingPayload payload;
    juce::String algorithm;
    juce::String manifestId;
    juce::String title;
    juce::String createdUtc;
    std::vector<std::uint8_t> manifestBytes;
};

class SoftBindingStore final
{
public:
    static constexpr const char* algorithm = "com.microsoft.wavmark.1";
    using PayloadGenerator = std::function<SoftBindingPayload()>;

    explicit SoftBindingStore(juce::File directory = defaultDirectory(),
                              PayloadGenerator generator = {});

    [[nodiscard]] static juce::File defaultDirectory();
    [[nodiscard]] juce::Result allocatePayload(SoftBindingPayload&) const;
    [[nodiscard]] juce::Result persist(const SoftBindingPayload&,
                                        const std::vector<std::uint8_t>& manifestBytes,
                                        const juce::String& title,
                                        juce::String& manifestId);
    [[nodiscard]] juce::Result resolve(const SoftBindingPayload&, StoredSoftBinding&) const;
    [[nodiscard]] bool contains(const SoftBindingPayload&) const;
    [[nodiscard]] const juce::File& directory() const noexcept { return root; }

private:
    [[nodiscard]] juce::Result readIndex(juce::var&) const;
    [[nodiscard]] juce::Result writeIndex(const juce::var&) const;
    [[nodiscard]] juce::String keyFor(const SoftBindingPayload&) const;
    [[nodiscard]] SoftBindingPayload randomPayload() const;

    juce::File root;
    PayloadGenerator payloadGenerator;
};
}

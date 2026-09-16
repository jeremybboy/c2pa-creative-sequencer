#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace c2paseq
{
inline constexpr std::string_view audioWMarkAlgorithm =
    "io.github.jeremybboy.audiowmark.1";

struct SoftBindingPayload
{
    static constexpr std::size_t byteCount = 16;
    std::array<std::uint8_t, byteCount> bytes {};

    [[nodiscard]] juce::String toHex() const;
    [[nodiscard]] juce::String toBase64() const;
    [[nodiscard]] juce::String toBitString() const;
    [[nodiscard]] static std::optional<SoftBindingPayload> fromHex(const juce::String&);
    [[nodiscard]] static std::optional<SoftBindingPayload> fromBitString(const juce::String&);

    bool operator==(const SoftBindingPayload&) const = default;
};
}

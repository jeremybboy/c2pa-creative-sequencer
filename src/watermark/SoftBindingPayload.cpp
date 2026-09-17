#include "SoftBindingPayload.h"

namespace c2paseq
{
juce::String SoftBindingPayload::toHex() const
{
    return juce::String::toHexString(bytes.data(), static_cast<int>(bytes.size()), 0)
        .toLowerCase();
}

juce::String SoftBindingPayload::toBase64() const
{
    return juce::Base64::toBase64(bytes.data(), bytes.size());
}

juce::String SoftBindingPayload::toBitString() const
{
    juce::String result;
    for (const auto byte : bytes)
        for (int bit = 7; bit >= 0; --bit)
            result += ((byte >> bit) & 1u) != 0 ? "1" : "0";
    return result;
}

std::optional<SoftBindingPayload> SoftBindingPayload::fromHex(const juce::String& input)
{
    const auto hex = input.trim().toLowerCase();
    if (hex.length() != static_cast<int>(byteCount * 2))
        return std::nullopt;
    for (const auto character : hex)
        if (juce::CharacterFunctions::getHexDigitValue(character) < 0)
            return std::nullopt;
    SoftBindingPayload output;
    for (int index = 0; index < static_cast<int>(byteCount); ++index)
        output.bytes[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(
            hex.substring(index * 2, index * 2 + 2).getHexValue32());
    return output;
}

std::optional<SoftBindingPayload> SoftBindingPayload::fromBitString(const juce::String& input)
{
    const auto bits = input.trim();
    if (bits.length() != static_cast<int>(byteCount * 8))
        return std::nullopt;
    for (const auto character : bits)
        if (character != '0' && character != '1')
            return std::nullopt;
    SoftBindingPayload output;
    for (int index = 0; index < static_cast<int>(byteCount * 8); ++index)
        if (bits[index] == '1')
            output.bytes[static_cast<std::size_t>(index / 8)] |= static_cast<std::uint8_t>(
                1u << (7 - index % 8));
    return output;
}
}

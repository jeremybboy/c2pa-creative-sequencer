#include "ComputerKeyboardMapping.h"

#include <algorithm>
#include <array>
#include <utility>

namespace c2paseq
{
namespace
{
constexpr std::array<std::pair<int, int>, 18> noteKeys {{
    { 'a', 0 }, { 'w', 1 }, { 's', 2 }, { 'e', 3 }, { 'd', 4 },
    { 'f', 5 }, { 't', 6 }, { 'g', 7 }, { 'y', 8 }, { 'h', 9 },
    { 'u', 10 }, { 'j', 11 }, { 'k', 12 }, { 'o', 13 }, { 'l', 14 },
    { 'p', 15 }, { ';', 16 }, { '\'', 17 }
}};

constexpr int normalise(int keyCode) noexcept
{
    return keyCode >= 'A' && keyCode <= 'Z' ? keyCode + ('a' - 'A') : keyCode;
}
}

std::optional<int> ComputerKeyboardMapping::semitoneForKey(int keyCode) noexcept
{
    const auto normalised = normalise(keyCode);
    const auto found = std::find_if(noteKeys.begin(), noteKeys.end(),
        [normalised](const auto& entry) { return entry.first == normalised; });
    return found == noteKeys.end() ? std::nullopt
                                   : std::optional<int>(found->second);
}

std::optional<int> ComputerKeyboardMapping::noteForKey(int keyCode,
                                                        int octaveOffset) noexcept
{
    const auto semitone = semitoneForKey(keyCode);
    if (! semitone.has_value())
        return std::nullopt;
    const auto note = baseMidiNote + clampOctaveOffset(octaveOffset) * 12 + *semitone;
    return note >= 0 && note <= 127 ? std::optional<int>(note) : std::nullopt;
}

bool ComputerKeyboardMapping::isOctaveDownKey(int keyCode) noexcept
{
    return normalise(keyCode) == 'z';
}

bool ComputerKeyboardMapping::isOctaveUpKey(int keyCode) noexcept
{
    return normalise(keyCode) == 'x';
}

int ComputerKeyboardMapping::clampOctaveOffset(int octaveOffset) noexcept
{
    return std::clamp(octaveOffset, minimumOctaveOffset, maximumOctaveOffset);
}

std::vector<int> ComputerKeyboardMapping::noteKeyCodes()
{
    std::vector<int> keys;
    keys.reserve(noteKeys.size());
    for (const auto& [key, semitone] : noteKeys)
    {
        (void) semitone;
        keys.push_back(key);
    }
    return keys;
}
}

#pragma once

#include <optional>
#include <vector>

namespace c2paseq
{
class ComputerKeyboardMapping final
{
public:
    static constexpr int baseMidiNote = 48;
    static constexpr int minimumOctaveOffset = -4;
    static constexpr int maximumOctaveOffset = 5;

    [[nodiscard]] static std::optional<int> semitoneForKey(int keyCode) noexcept;
    [[nodiscard]] static std::optional<int> noteForKey(int keyCode,
                                                       int octaveOffset) noexcept;
    [[nodiscard]] static bool isOctaveDownKey(int keyCode) noexcept;
    [[nodiscard]] static bool isOctaveUpKey(int keyCode) noexcept;
    [[nodiscard]] static int clampOctaveOffset(int octaveOffset) noexcept;
    [[nodiscard]] static std::vector<int> noteKeyCodes();
};
}

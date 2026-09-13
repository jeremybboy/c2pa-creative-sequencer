#pragma once

#include "TracktionAdapter.h"

namespace c2paseq
{
class AudioEngine final
{
public:
    AudioEngine() = default;

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String status() const;

private:
    TracktionAdapter tracktion;
};
}

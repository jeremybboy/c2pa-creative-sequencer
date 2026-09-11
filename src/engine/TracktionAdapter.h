#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace c2paseq
{
class TracktionAdapter final
{
public:
    TracktionAdapter();

    [[nodiscard]] bool isInitialised() const noexcept;
    [[nodiscard]] juce::String audioDeviceDescription() const;

private:
    tracktion::engine::Engine engine;
    bool initialised = false;
};
}

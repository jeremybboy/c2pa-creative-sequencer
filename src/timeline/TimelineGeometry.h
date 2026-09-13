#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
class TimelineGeometry
{
public:
    static constexpr int beatsPerBar = 4;

    double bpm = 120.0;
    double pixelsPerSecond = 96.0;
    double scrollSeconds = 0.0;

    [[nodiscard]] double beatSeconds() const noexcept;
    [[nodiscard]] double barSeconds() const noexcept;
    [[nodiscard]] double timeToX(double seconds) const noexcept;
    [[nodiscard]] double xToTime(double x) const noexcept;
    [[nodiscard]] double snapToBeat(double seconds) const noexcept;
    void zoomAround(double newPixelsPerSecond, double anchorX) noexcept;
};
}

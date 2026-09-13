#include "TimelineGeometry.h"

#include <cmath>

namespace c2paseq
{
double TimelineGeometry::beatSeconds() const noexcept
{
    return 60.0 / juce::jlimit(40.0, 240.0, bpm);
}

double TimelineGeometry::barSeconds() const noexcept
{
    return beatSeconds() * beatsPerBar;
}

double TimelineGeometry::timeToX(double seconds) const noexcept
{
    return (seconds - scrollSeconds) * pixelsPerSecond;
}

double TimelineGeometry::xToTime(double x) const noexcept
{
    return std::max(0.0, scrollSeconds + x / pixelsPerSecond);
}

double TimelineGeometry::snapToBeat(double seconds) const noexcept
{
    const auto beat = beatSeconds();
    return std::max(0.0, std::round(seconds / beat) * beat);
}

void TimelineGeometry::zoomAround(double newPixelsPerSecond, double anchorX) noexcept
{
    const auto anchorTime = xToTime(anchorX);
    pixelsPerSecond = juce::jlimit(24.0, 640.0, newPixelsPerSecond);
    scrollSeconds = std::max(0.0, anchorTime - anchorX / pixelsPerSecond);
}
}

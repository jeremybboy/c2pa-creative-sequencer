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
    pixelsPerSecond = juce::jlimit(minimumZoom, maximumZoom, newPixelsPerSecond);
    scrollSeconds = std::max(0.0, anchorTime - anchorX / pixelsPerSecond);
}

double TimelineGeometry::snapStepBeats() const noexcept
{
    // Keep grid lines at least 24 pixels apart; smallest step is 1/8 beat
    // (a 1/32 note in 4/4). This depends on BPM, not just screen zoom.
    const auto pixelsPerBeat = pixelsPerSecond * beatSeconds();
    if (pixelsPerBeat < 24.0) return 4.0;
    if (pixelsPerBeat < 48.0) return 1.0;
    if (pixelsPerBeat < 96.0) return 0.5;
    if (pixelsPerBeat < 192.0) return 0.25;
    return 0.125;
}

double TimelineGeometry::snapStepSeconds() const noexcept
{
    return beatSeconds() * snapStepBeats();
}

double TimelineGeometry::snapToGrid(double seconds) const noexcept
{
    const auto step = snapStepSeconds();
    return std::max(0.0, std::round(seconds / step) * step);
}
}

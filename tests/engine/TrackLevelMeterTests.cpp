#include "engine/TrackLevelMeter.h"

#include <cmath>
#include <iostream>

namespace
{
int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}

bool close(float left, float right, float tolerance = 0.0001f)
{
    return std::abs(left - right) <= tolerance;
}
}

int main()
{
    c2paseq::TrackLevelAccumulator accumulator;
    juce::AudioBuffer<float> buffer(2, 8);
    buffer.clear();
    buffer.setSample(0, 2, 0.25f);
    buffer.setSample(0, 4, -0.5f);
    buffer.setSample(1, 3, 0.75f);
    accumulator.process(buffer, 0, buffer.getNumSamples());

    const auto measured = accumulator.consume();
    if (! close(measured.left, 0.5f) || ! close(measured.right, 0.75f))
        return fail(1, "stereo peak accumulator did not report actual signal levels");
    const auto cleared = accumulator.consume();
    if (cleared.left != 0.0f || cleared.right != 0.0f)
        return fail(2, "peak handoff did not clear consumed realtime values");

    juce::AudioBuffer<float> mono(1, 4);
    mono.clear();
    mono.setSample(0, 1, -0.4f);
    accumulator.process(mono, 0, mono.getNumSamples());
    const auto monoMeasured = accumulator.consume();
    if (! close(monoMeasured.left, 0.4f) || ! close(monoMeasured.right, 0.4f))
        return fail(3, "mono signal was not mirrored into both meter channels");

    c2paseq::TrackLevelBallistics ballistics;
    const auto active = ballistics.update({ 1.0f, 0.25f }, true);
    if (! close(active.left, 1.0f) || active.right <= 0.0f || active.right >= 1.0f)
        return fail(4, "meter scale did not map real peaks into the display range");
    const auto decaying = ballistics.update({}, false);
    if (! (decaying.left > 0.0f && decaying.left < active.left)
        || ! (decaying.right > 0.0f && decaying.right < active.right))
        return fail(5, "inaudible meter did not begin a readable decay");

    for (int i = 0; i < 120; ++i)
    {
        const auto ignored = ballistics.update({}, false);
        juce::ignoreUnused(ignored);
    }
    const auto silent = ballistics.current();
    if (silent.left > 0.000001f || silent.right > 0.000001f)
        return fail(6, "stopped or muted meter did not decay to silence");

    std::cout << "realtime-safe track peak handoff and meter ballistics passed\n";
    return 0;
}

#include "midi/MidiNoteFormatting.h"
#include "timeline/TimelineGeometry.h"
#include "ui/InputMenuGrouping.h"
#include "ui/PianoRollView.h"
#include "ui/TrackHeaderView.h"
#include "ui/SequencerLookAndFeel.h"

#include <cmath>
#include <iostream>
#include <set>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    using namespace c2paseq;
    const auto close = [](double left, double right) { return std::abs(left - right) < 0.000001; };
    const std::pair<int, const char*> pitches[] {
        { 0, "C-1" }, { 12, "C0" }, { 48, "C3" }, { 49, "C#3" }, { 50, "D3" },
        { 54, "F#3" }, { 58, "A#3" }, { 59, "B3" }, { 60, "C4" }, { 61, "C#4" },
        { 62, "D4" }, { 66, "F#4" }, { 70, "A#4" }, { 71, "B4" }, { 127, "G9" }
    };
    for (const auto& [number, expected] : pitches)
        if (midiNoteName(number) != expected) return 1;
    if (midiNoteName(-1).isNotEmpty() || midiNoteName(128).isNotEmpty()) return 2;

    TimelineGeometry geometry;
    for (const auto bpm : { 40.0, 120.0, 127.0, 240.0 })
    {
        geometry.bpm = bpm;
        for (const auto& [pixelsPerBeat, step] : {
            std::pair{ 12.0, 4.0 }, { 30.0, 1.0 }, { 60.0, 0.5 },
            { 120.0, 0.25 }, { 240.0, 0.125 } })
        {
            geometry.pixelsPerSecond = pixelsPerBeat / geometry.beatSeconds();
            if (! close(geometry.snapStepBeats(), step)) return 3;
            const auto seconds = 7.31 * geometry.beatSeconds();
            const auto snapped = geometry.snapToGrid(seconds);
            if (! close(snapped, std::round(7.31 / step) * step * geometry.beatSeconds())
                || ! close(geometry.snapToGrid(snapped), snapped)
                || ! close(geometry.snapToGrid(-1.0), 0.0)) return 4;
        }
    }
    geometry.scrollSeconds = 3.0;
    const auto anchorTime = geometry.xToTime(300.0);
    geometry.zoomAround(100000.0, 300.0);
    if (! close(geometry.pixelsPerSecond, TimelineGeometry::maximumZoom)
        || ! close(geometry.xToTime(300.0), anchorTime)) return 5;
    geometry.zoomAround(1.0, 0.0);
    if (! close(geometry.pixelsPerSecond, TimelineGeometry::minimumZoom)) return 6;

    std::vector<AudioInputChoice> inputs;
    for (int channel = 0; channel < 64; ++channel)
        inputs.push_back({ "Many-channel source", channel, "Channel " + juce::String(channel + 1) });
    inputs.push_back({ "Built-in microphone", 0, "Microphone" });
    inputs.push_back({ "Interface", 1, "Input 2" });
    inputs.push_back({ "Interface", 0, "Input 1" });
    const auto groups = groupRecordingInputs(inputs);
    if (groups.size() != 3 || groups[0].deviceName != "Built-in microphone"
        || groups[0].advanced() || groups[1].advanced() || ! groups[2].advanced()) return 7;
    std::set<std::size_t> exposedIndices;
    for (const auto& group : groups)
        for (const auto index : group.inputIndices)
            if (! exposedIndices.insert(index).second || inputs[index].deviceName != group.deviceName) return 8;
    if (exposedIndices.size() != inputs.size() || ! groupRecordingInputs({}).empty()) return 9;
    auto selected = inputs[66];
    std::reverse(inputs.begin(), inputs.end());
    if (std::count_if(inputs.begin(), inputs.end(), [&](const auto& input)
        { return sameRecordingInput(input, selected); }) != 1) return 10;

    // Offscreen component render is not live DAW/hardware acceptance.
    juce::Image preview(juce::Image::RGB, 1280, 550, true);
    juce::Graphics graphics(preview);
    graphics.fillAll(juce::Colour::fromRGB(23, 28, 34));
    SequencerLookAndFeel lookAndFeel;
    for (int index = 0; index < 3; ++index)
    {
        TrackHeaderView header(index);
        header.setLookAndFeel(&lookAndFeel);
        header.setSize(268, 116);
        header.setState(index == 1 ? "Keys" : "A long but proportioned audio track name",
            -6.0, 0.0, false, false, index == 1 ? TrackType::midi : TrackType::audio,
            juce::Colour::fromRGB(80, 170, 190));
        header.setRecordArmed(false, "Test input");
        header.setPluginState(TrackPluginSnapshot { "test", index == 1 ? "Surge XT"
            : "A very long plug-in name that must truncate cleanly", "Test vendor", false, index == 2 }, {});
        for (auto* child : header.getChildren())
            if (child->isVisible() && (child->getWidth() <= 0 || child->getHeight() <= 0
                || ! header.getLocalBounds().contains(child->getBounds()))) return 11;
        graphics.drawImageAt(header.createComponentSnapshot(header.getLocalBounds()), 12, 42 + index * 128);
        header.setLookAndFeel(nullptr);
    }
    PianoRollView piano;
    piano.setLookAndFeel(&lookAndFeel);
    piano.setSize(972, 460);
    ArrangementMidiClipSnapshot midi;
    midi.id = "test-clip";
    midi.startBeats = 8.0;
    midi.lengthBeats = 8.0;
    midi.notes = { { "C", 60, 0.0, 1.0, 100 }, { "D", 62, 1.0, 1.0, 100 },
        { "F#", 66, 2.0, 1.0, 100 }, { "A#", 70, 3.0, 1.0, 100 }, { "B", 71, 4.0, 1.0, 100 } };
    piano.setClip(midi);
    piano.setPlayheadBeat(10.5);
    graphics.drawImageAt(piano.createComponentSnapshot(piano.getLocalBounds()), 296, 42);
    piano.setLookAndFeel(nullptr);
    if (argc == 3 && juce::String(argv[1]) == "--render-ui")
    {
        const juce::File output(argv[2]);
        auto stream = output.createOutputStream();
        if (stream == nullptr || ! juce::PNGImageFormat().writeImageToStream(preview, *stream)) return 12;
    }
    std::cout << "musical pitch names, BPM-aware adaptive grid, deep zoom, complete grouped inputs, stable selection and header layout/render passed\n";
    return 0;
}

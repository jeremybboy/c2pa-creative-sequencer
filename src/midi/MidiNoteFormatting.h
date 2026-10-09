#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
inline juce::String midiNoteName(int number)
{
    if (number < 0 || number > 127) return {};
    static constexpr const char* names[] {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    // Existing piano-roll convention: MIDI 60 = C4.
    return juce::String(names[number % 12]) + juce::String(number / 12 - 1);
}
}

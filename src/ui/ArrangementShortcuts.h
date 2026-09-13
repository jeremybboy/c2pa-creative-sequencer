#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
enum class ArrangementCommand
{
    none,
    togglePlayPause,
    save,
    undo,
    redo,
    duplicateClip,
    splitClip,
    deleteClip,
    zoomIn,
    zoomOut
};

[[nodiscard]] ArrangementCommand commandForKeyPress(const juce::KeyPress& key);
}

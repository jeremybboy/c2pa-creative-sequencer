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
    selectAll,
    copy,
    cut,
    paste,
    duplicateClip,
    splitClip,
    deleteClip,
    loopSelection,
    zoomIn,
    zoomOut
};

[[nodiscard]] ArrangementCommand commandForKeyPress(const juce::KeyPress& key);
}

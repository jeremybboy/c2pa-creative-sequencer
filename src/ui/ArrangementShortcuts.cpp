#include "ArrangementShortcuts.h"

namespace c2paseq
{
ArrangementCommand commandForKeyPress(const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto modifiers = key.getModifiers();
    const auto command = modifiers.isCommandDown();
    const auto shift = modifiers.isShiftDown();

    if (code == juce::KeyPress::spaceKey && ! command)
        return ArrangementCommand::togglePlayPause;
    if (command && (code == 's' || code == 'S'))
        return ArrangementCommand::save;
    if (command && (code == 'z' || code == 'Z'))
        return shift ? ArrangementCommand::redo : ArrangementCommand::undo;
    if (command && (code == 'a' || code == 'A'))
        return ArrangementCommand::selectAll;
    if (command && (code == 'c' || code == 'C'))
        return ArrangementCommand::copy;
    if (command && (code == 'x' || code == 'X'))
        return ArrangementCommand::cut;
    if (command && (code == 'v' || code == 'V'))
        return ArrangementCommand::paste;
    if (command && (code == 'd' || code == 'D'))
        return ArrangementCommand::duplicateClip;
    if (command && (code == 'e' || code == 'E'))
        return ArrangementCommand::splitClip;
    if (command && (code == 'l' || code == 'L'))
        return ArrangementCommand::loopSelection;
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        return ArrangementCommand::deleteClip;
    if (code == '+' || code == '=')
        return ArrangementCommand::zoomIn;
    if (code == '-' || code == '_')
        return ArrangementCommand::zoomOut;
    return ArrangementCommand::none;
}
}

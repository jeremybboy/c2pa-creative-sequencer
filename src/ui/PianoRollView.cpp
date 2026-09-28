#include "PianoRollView.h"

#include <algorithm>
#include <cmath>

namespace c2paseq
{
PianoRollView::PianoRollView()
{
    setWantsKeyboardFocus(true);
}

void PianoRollView::setClip(ArrangementMidiClipSnapshot clip)
{
    midiClip = std::move(clip);
    std::erase_if(selectedNotes, [this](const auto& id)
    {
        return std::none_of(midiClip.notes.begin(), midiClip.notes.end(),
            [&](const auto& note) { return note.id == id; });
    });
    visibleStartBeat = juce::jlimit(0.0,
        std::max(0.0, midiClip.lengthBeats - visibleBeatLength()), visibleStartBeat);
    repaint();
}

juce::Rectangle<float> PianoRollView::gridBounds() const
{
    return getLocalBounds().toFloat().withTrimmedTop(static_cast<float>(headerHeight))
        .withTrimmedLeft(static_cast<float>(keyboardWidth))
        .withTrimmedBottom(static_cast<float>(velocityHeight));
}

juce::Rectangle<float> PianoRollView::velocityBounds() const
{
    return getLocalBounds().toFloat().withTrimmedTop(
        static_cast<float>(getHeight() - velocityHeight))
        .withTrimmedLeft(static_cast<float>(keyboardWidth));
}

int PianoRollView::highestVisiblePitch() const noexcept
{
    return lowestVisiblePitch + visiblePitchCount - 1;
}

double PianoRollView::visibleBeatLength() const noexcept
{
    return midiClip.lengthBeats / std::max(1.0, horizontalZoom);
}

juce::Rectangle<float> PianoRollView::noteBounds(
    const ArrangementMidiNoteSnapshot& note) const
{
    const auto grid = gridBounds();
    const auto rowHeight = grid.getHeight() / static_cast<float>(visiblePitchCount);
    const auto beatLength = std::max(gridStepBeats, visibleBeatLength());
    const auto x = grid.getX() + static_cast<float>(
        (note.startBeats - visibleStartBeat) / beatLength) * grid.getWidth();
    const auto width = std::max(4.0f, static_cast<float>(
        note.durationBeats / beatLength) * grid.getWidth());
    const auto y = grid.getY() + static_cast<float>(
        highestVisiblePitch() - note.noteNumber) * rowHeight;
    return { x, y, width, std::max(3.0f, rowHeight) };
}

const ArrangementMidiNoteSnapshot* PianoRollView::noteAt(juce::Point<float> point) const
{
    for (auto iterator = midiClip.notes.rbegin(); iterator != midiClip.notes.rend(); ++iterator)
        if (iterator->noteNumber >= lowestVisiblePitch
            && iterator->noteNumber <= highestVisiblePitch()
            && noteBounds(*iterator).expanded(1.0f).contains(point))
            return &*iterator;
    return nullptr;
}

int PianoRollView::pitchAt(float y) const
{
    const auto grid = gridBounds();
    const auto rowHeight = grid.getHeight() / static_cast<float>(visiblePitchCount);
    return juce::jlimit(0, 127, highestVisiblePitch()
        - static_cast<int>((y - grid.getY()) / std::max(1.0f, rowHeight)));
}

double PianoRollView::beatAt(float x, bool snap) const
{
    const auto grid = gridBounds();
    const auto raw = visibleStartBeat + (x - grid.getX())
        / std::max(1.0f, grid.getWidth()) * visibleBeatLength();
    return juce::jlimit(0.0, midiClip.lengthBeats,
        snap ? std::round(raw / gridStepBeats) * gridStepBeats : raw);
}

int PianoRollView::velocityAt(float y) const
{
    const auto lane = velocityBounds();
    return juce::jlimit(1, 127, juce::roundToInt(
        (lane.getBottom() - y) / std::max(1.0f, lane.getHeight()) * 126.0f + 1.0f));
}

const ArrangementMidiNoteSnapshot* PianoRollView::selectedNote() const
{
    if (selectedNotes.size() != 1)
        return nullptr;
    const auto found = std::find_if(midiClip.notes.begin(), midiClip.notes.end(),
        [&](const auto& note) { return note.id == *selectedNotes.begin(); });
    return found == midiClip.notes.end() ? nullptr : &*found;
}

std::vector<juce::String> PianoRollView::selectedNoteIds() const
{
    return { selectedNotes.begin(), selectedNotes.end() };
}

void PianoRollView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(29, 33, 38));
    g.setColour(juce::Colour::fromRGB(18, 24, 31));
    g.fillRect(0, 0, getWidth(), headerHeight);
    g.setColour(juce::Colour::fromRGB(232, 236, 239));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Piano Roll  •  double-click draw  •  drag/resize  •  Cmd-scroll zoom",
               14, 0, getWidth() - 62, headerHeight, juce::Justification::centredLeft);
    g.setColour(juce::Colour::fromRGB(76, 86, 96));
    g.fillRoundedRectangle(static_cast<float>(getWidth() - 42), 7.0f, 32.0f, 24.0f, 4.0f);
    g.setColour(juce::Colours::white);
    g.drawText("×", getWidth() - 42, 6, 32, 24, juce::Justification::centred);

    const auto grid = gridBounds();
    const auto rowHeight = grid.getHeight() / static_cast<float>(visiblePitchCount);
    g.setColour(juce::Colour::fromRGB(47, 52, 58));
    g.fillRect(grid);
    for (auto pitch = lowestVisiblePitch; pitch <= highestVisiblePitch(); ++pitch)
    {
        const auto y = grid.getY() + static_cast<float>(highestVisiblePitch() - pitch) * rowHeight;
        const auto black = pitch % 12 == 1 || pitch % 12 == 3 || pitch % 12 == 6
            || pitch % 12 == 8 || pitch % 12 == 10;
        g.setColour(black ? juce::Colour::fromRGB(33, 37, 42)
                          : juce::Colour::fromRGB(58, 63, 69));
        g.fillRect(0.0f, y, static_cast<float>(keyboardWidth), rowHeight);
        g.setColour(juce::Colour::fromRGB(72, 77, 83));
        g.drawHorizontalLine(juce::roundToInt(y), 0.0f, grid.getRight());
        if (pitch % 12 == 0)
        {
            g.setColour(juce::Colour::fromRGB(210, 214, 217));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText("C" + juce::String(pitch / 12 - 1), 5,
                       juce::roundToInt(y), keyboardWidth - 8,
                       std::max(8, juce::roundToInt(rowHeight)),
                       juce::Justification::centredLeft);
        }
    }

    const auto visibleEnd = visibleStartBeat + visibleBeatLength();
    const auto firstGridBeat = std::floor(visibleStartBeat / gridStepBeats) * gridStepBeats;
    for (double beat = firstGridBeat; beat <= visibleEnd + 0.0001; beat += gridStepBeats)
    {
        const auto x = grid.getX() + static_cast<float>(
            (beat - visibleStartBeat) / std::max(gridStepBeats, visibleBeatLength()))
            * grid.getWidth();
        const auto wholeBeat = std::abs(std::round(beat) - beat) < 0.0001;
        const auto bar = std::abs(std::round(beat / 4.0) * 4.0 - beat) < 0.0001;
        g.setColour(bar ? juce::Colour::fromRGB(120, 130, 140)
                        : wholeBeat ? juce::Colour::fromRGB(93, 101, 109)
                                    : juce::Colour::fromRGB(66, 72, 78));
        g.drawVerticalLine(juce::roundToInt(x), grid.getY(), grid.getBottom());
    }

    for (const auto& note : midiClip.notes)
    {
        if (note.noteNumber < lowestVisiblePitch || note.noteNumber > highestVisiblePitch()
            || note.startBeats + note.durationBeats < visibleStartBeat
            || note.startBeats > visibleEnd)
            continue;
        const auto bounds = noteBounds(note).reduced(0.5f).getIntersection(grid);
        g.setColour(selectedNotes.contains(note.id)
            ? juce::Colour::fromRGB(255, 213, 92)
            : juce::Colour::fromRGB(91, 211, 143));
        g.fillRoundedRectangle(bounds, 2.0f);
        g.setColour(juce::Colour::fromRGB(18, 69, 44));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }

    const auto lane = velocityBounds();
    g.setColour(juce::Colour::fromRGB(24, 28, 33));
    g.fillRect(lane);
    g.setColour(juce::Colour::fromRGB(124, 132, 140));
    g.drawText("VELOCITY", 5, juce::roundToInt(lane.getY()), keyboardWidth - 8,
               velocityHeight, juce::Justification::centred);
    for (const auto& note : midiClip.notes)
    {
        if (note.startBeats < visibleStartBeat || note.startBeats > visibleEnd)
            continue;
        const auto x = lane.getX() + static_cast<float>(
            (note.startBeats - visibleStartBeat) / std::max(gridStepBeats, visibleBeatLength()))
            * lane.getWidth();
        const auto height = static_cast<float>(note.velocity) / 127.0f * (lane.getHeight() - 8.0f);
        g.setColour(selectedNotes.contains(note.id)
            ? juce::Colour::fromRGB(255, 213, 92)
            : juce::Colour::fromRGB(91, 211, 143));
        g.fillRect(x, lane.getBottom() - height, 4.0f, height);
    }
    g.setColour(juce::Colour::fromRGB(115, 124, 132));
    g.drawRect(getLocalBounds().toFloat(), 1.0f);
}

void PianoRollView::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    dragMode = DragMode::none;
    dragStart = event.position;
    if (juce::Rectangle<int>(getWidth() - 42, 7, 32, 24).contains(event.getPosition()))
    {
        if (onClose) onClose();
        return;
    }
    if (const auto* note = noteAt(event.position))
    {
        if (event.mods.isCommandDown())
        {
            if (selectedNotes.contains(note->id))
                selectedNotes.erase(note->id);
            else
                selectedNotes.insert(note->id);
        }
        else if (! selectedNotes.contains(note->id))
        {
            selectedNotes.clear();
            selectedNotes.insert(note->id);
        }
        dragNote = *note;
        dragMode = event.position.x >= noteBounds(*note).getRight() - 7.0f
            ? DragMode::resize : DragMode::move;
        repaint();
        return;
    }
    if (velocityBounds().contains(event.position) && ! midiClip.notes.empty())
    {
        const auto beat = beatAt(event.position.x);
        const auto found = std::min_element(midiClip.notes.begin(), midiClip.notes.end(),
            [&](const auto& left, const auto& right)
            {
                return std::abs(left.startBeats - beat) < std::abs(right.startBeats - beat);
            });
        selectedNotes.clear();
        selectedNotes.insert(found->id);
        dragNote = *found;
        dragMode = DragMode::velocity;
        repaint();
        return;
    }
    if (! event.mods.isCommandDown())
    {
        selectedNotes.clear();
        repaint();
    }
}

void PianoRollView::mouseDrag(const juce::MouseEvent&)
{
}

void PianoRollView::mouseUp(const juce::MouseEvent& event)
{
    if (dragMode == DragMode::none || ! onUpdateNote)
        return;
    auto noteNumber = dragNote.noteNumber;
    auto startBeats = dragNote.startBeats;
    auto durationBeats = dragNote.durationBeats;
    auto velocity = dragNote.velocity;
    const auto grid = gridBounds();
    const auto pixelsPerBeat = grid.getWidth() / static_cast<float>(
        std::max(gridStepBeats, visibleBeatLength()));
    auto beatDelta = static_cast<double>(event.position.x - dragStart.x)
        / static_cast<double>(std::max(1.0f, pixelsPerBeat));
    if (! event.mods.isAltDown())
        beatDelta = std::round(beatDelta / gridStepBeats) * gridStepBeats;
    if (dragMode == DragMode::move)
    {
        startBeats = juce::jlimit(0.0,
            std::max(0.0, midiClip.lengthBeats - durationBeats),
            dragNote.startBeats + beatDelta);
        noteNumber = juce::jlimit(0, 127,
            dragNote.noteNumber + pitchAt(event.position.y) - pitchAt(dragStart.y));
    }
    else if (dragMode == DragMode::resize)
    {
        durationBeats = juce::jlimit(event.mods.isAltDown() ? 0.01 : gridStepBeats,
            std::max(gridStepBeats, midiClip.lengthBeats - startBeats),
            dragNote.durationBeats + beatDelta);
    }
    else if (dragMode == DragMode::velocity)
    {
        velocity = velocityAt(event.position.y);
    }
    dragMode = DragMode::none;
    onUpdateNote(dragNote.id, noteNumber, startBeats, durationBeats, velocity);
}

void PianoRollView::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! gridBounds().contains(event.position) || noteAt(event.position) != nullptr
        || ! onAddNote)
        return;
    const auto start = std::min(beatAt(event.position.x, ! event.mods.isAltDown()),
        std::max(0.0, midiClip.lengthBeats - gridStepBeats));
    const auto duration = std::min(1.0, midiClip.lengthBeats - start);
    onAddNote(pitchAt(event.position.y), start, duration, 100);
}

void PianoRollView::mouseWheelMove(const juce::MouseEvent& event,
                                   const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown())
    {
        const auto anchor = beatAt(event.position.x, false);
        const auto proportion = (anchor - visibleStartBeat)
            / std::max(gridStepBeats, visibleBeatLength());
        horizontalZoom = juce::jlimit(1.0, 8.0,
            horizontalZoom * (wheel.deltaY > 0.0f ? 1.25 : 0.8));
        visibleStartBeat = anchor - proportion * visibleBeatLength();
        visibleStartBeat = juce::jlimit(0.0,
            std::max(0.0, midiClip.lengthBeats - visibleBeatLength()), visibleStartBeat);
    }
    else if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    {
        visibleStartBeat = juce::jlimit(0.0,
            std::max(0.0, midiClip.lengthBeats - visibleBeatLength()),
            visibleStartBeat - static_cast<double>(wheel.deltaX + wheel.deltaY)
                * visibleBeatLength() * 0.5);
    }
    else
    {
        lowestVisiblePitch = juce::jlimit(0, 128 - visiblePitchCount,
            lowestVisiblePitch + juce::roundToInt(wheel.deltaY * 8.0f));
    }
    repaint();
}

void PianoRollView::copySelectedNotes(bool cut)
{
    if (selectedNotes.empty())
        return;
    noteClipboard.clear();
    auto minimumStart = midiClip.lengthBeats;
    auto maximumEnd = 0.0;
    for (const auto& note : midiClip.notes)
        if (selectedNotes.contains(note.id))
        {
            noteClipboard.push_back(note);
            minimumStart = std::min(minimumStart, note.startBeats);
            maximumEnd = std::max(maximumEnd, note.startBeats + note.durationBeats);
        }
    for (auto& note : noteClipboard)
        note.startBeats -= minimumStart;
    clipboardSpanBeats = maximumEnd - minimumStart;
    pasteCursorBeats = std::min(midiClip.lengthBeats - clipboardSpanBeats, maximumEnd);
    if (cut && onDeleteNotes)
        onDeleteNotes(selectedNoteIds());
    else if (onStatus)
        onStatus("Copied " + juce::String(static_cast<int>(noteClipboard.size()))
            + (noteClipboard.size() == 1 ? " MIDI note" : " MIDI notes"));
}

void PianoRollView::insertClipboard(bool duplicate)
{
    if (duplicate)
        copySelectedNotes(false);
    if (noteClipboard.empty() || ! onInsertNotes)
        return;
    const auto target = juce::jlimit(0.0,
        std::max(0.0, midiClip.lengthBeats - clipboardSpanBeats), pasteCursorBeats);
    auto notes = noteClipboard;
    for (auto& note : notes)
    {
        note.id.clear();
        note.startBeats += target;
    }
    pasteCursorBeats = std::min(midiClip.lengthBeats - clipboardSpanBeats,
                                target + std::max(1.0, clipboardSpanBeats));
    onInsertNotes(notes);
}

bool PianoRollView::keyPressed(const juce::KeyPress& key)
{
    if ((key.getKeyCode() == juce::KeyPress::deleteKey
         || key.getKeyCode() == juce::KeyPress::backspaceKey)
        && ! selectedNotes.empty() && onDeleteNotes)
    {
        const auto ids = selectedNoteIds();
        selectedNotes.clear();
        onDeleteNotes(ids);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    if (! key.getModifiers().isCommandDown())
        return false;
    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    if (character == 'a')
    {
        selectedNotes.clear();
        for (const auto& note : midiClip.notes)
            selectedNotes.insert(note.id);
        repaint();
        return true;
    }
    if (character == 'c')
    {
        copySelectedNotes(false);
        return true;
    }
    if (character == 'x')
    {
        copySelectedNotes(true);
        return true;
    }
    if (character == 'v')
    {
        insertClipboard(false);
        return true;
    }
    if (character == 'd')
    {
        insertClipboard(true);
        return true;
    }
    return false;
}
}

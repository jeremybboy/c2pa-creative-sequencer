#include "engine/ClipOcclusion.h"
#include "ui/ArrangementShortcuts.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
bool close(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}

int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}

c2paseq::ClipModel clip(double start, double offset, double length)
{
    c2paseq::ClipModel value;
    value.startSeconds = start;
    value.sourceOffsetSeconds = offset;
    value.lengthSeconds = length;
    return value;
}

bool overlaps(const c2paseq::PlaybackClipSegment& left,
              const c2paseq::PlaybackClipSegment& right)
{
    const auto leftEnd = left.startSeconds + left.lengthSeconds;
    const auto rightEnd = right.startSeconds + right.lengthSeconds;
    return left.startSeconds < rightEnd && right.startSeconds < leftEnd;
}
}

int main()
{
    using c2paseq::ArrangementCommand;
    using c2paseq::buildPlaybackClipSegments;

    std::vector<c2paseq::ClipModel> sameTrack {
        clip(0.0, 0.0, 10.0),
        clip(3.0, 1.0, 4.0)
    };
    const auto occluded = buildPlaybackClipSegments(sameTrack);
    if (occluded.size() != 3
        || occluded[0].clipIndex != 0 || ! close(occluded[0].startSeconds, 0.0)
        || ! close(occluded[0].sourceOffsetSeconds, 0.0)
        || ! close(occluded[0].lengthSeconds, 3.0)
        || occluded[1].clipIndex != 0 || ! close(occluded[1].startSeconds, 7.0)
        || ! close(occluded[1].sourceOffsetSeconds, 7.0)
        || ! close(occluded[1].lengthSeconds, 3.0)
        || occluded[2].clipIndex != 1 || ! close(occluded[2].startSeconds, 3.0)
        || ! close(occluded[2].sourceOffsetSeconds, 1.0)
        || ! close(occluded[2].lengthSeconds, 4.0))
        return fail(1, "later same-track clip did not occlude only the overlap");

    for (std::size_t left = 0; left < occluded.size(); ++left)
        for (std::size_t right = left + 1; right < occluded.size(); ++right)
            if (overlaps(occluded[left], occluded[right]))
                return fail(2, "same-track playback segments still overlap");

    sameTrack.pop_back();
    const auto restored = buildPlaybackClipSegments(sameTrack);
    if (restored.size() != 1 || ! close(restored[0].startSeconds, 0.0)
        || ! close(restored[0].lengthSeconds, 10.0))
        return fail(3, "deleting the priority clip did not restore underlying audio");

    const auto firstTrack = buildPlaybackClipSegments({ clip(0.0, 0.0, 10.0) });
    const auto secondTrack = buildPlaybackClipSegments({ clip(3.0, 0.0, 4.0) });
    if (firstTrack.size() != 1 || secondTrack.size() != 1
        || ! overlaps(firstTrack[0], secondTrack[0]))
        return fail(4, "different tracks did not retain simultaneous playback ranges");

    const auto noModifiers = juce::ModifierKeys::noModifiers;
    const auto command = juce::ModifierKeys::commandModifier;
    const auto commandShift = juce::ModifierKeys(
        juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
    if (c2paseq::commandForKeyPress({ juce::KeyPress::spaceKey, noModifiers, ' ' })
            != ArrangementCommand::togglePlayPause
        || c2paseq::commandForKeyPress({ 's', command, 's' }) != ArrangementCommand::save
        || c2paseq::commandForKeyPress({ 'z', command, 'z' }) != ArrangementCommand::undo
        || c2paseq::commandForKeyPress({ 'z', commandShift, 'z' }) != ArrangementCommand::redo
        || c2paseq::commandForKeyPress({ 'd', command, 'd' })
            != ArrangementCommand::duplicateClip
        || c2paseq::commandForKeyPress({ 'e', command, 'e' })
            != ArrangementCommand::splitClip
        || c2paseq::commandForKeyPress(
               { juce::KeyPress::deleteKey, noModifiers, 0 })
            != ArrangementCommand::deleteClip)
        return fail(5, "arrangement keyboard mapping is incomplete");

    std::cout << "arrangement rules: overlap priority, restoration, cross-track mixing, "
                 "and shortcuts passed\n";
    return 0;
}

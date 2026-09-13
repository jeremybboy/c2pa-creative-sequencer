#include "ClipOcclusion.h"

#include <algorithm>

namespace c2paseq
{
namespace
{
constexpr double minimumSegmentLength = 0.000001;

struct TimeRange
{
    double start = 0.0;
    double end = 0.0;
};

std::vector<TimeRange> subtractRange(const std::vector<TimeRange>& ranges,
                                     TimeRange occluder)
{
    std::vector<TimeRange> result;
    for (const auto range : ranges)
    {
        if (occluder.end <= range.start || occluder.start >= range.end)
        {
            result.push_back(range);
            continue;
        }

        if (occluder.start - range.start > minimumSegmentLength)
            result.push_back({ range.start, std::min(range.end, occluder.start) });
        if (range.end - occluder.end > minimumSegmentLength)
            result.push_back({ std::max(range.start, occluder.end), range.end });
    }
    return result;
}
}

std::vector<PlaybackClipSegment> buildPlaybackClipSegments(
    const std::vector<ClipModel>& clips)
{
    std::vector<PlaybackClipSegment> segments;

    for (std::size_t clipIndex = 0; clipIndex < clips.size(); ++clipIndex)
    {
        const auto& clip = clips[clipIndex];
        if (clip.lengthSeconds <= minimumSegmentLength)
            continue;

        std::vector<TimeRange> audibleRanges {
            { clip.startSeconds, clip.startSeconds + clip.lengthSeconds }
        };

        for (std::size_t priorityIndex = clipIndex + 1;
             priorityIndex < clips.size() && ! audibleRanges.empty();
             ++priorityIndex)
        {
            const auto& priorityClip = clips[priorityIndex];
            if (priorityClip.lengthSeconds <= minimumSegmentLength)
                continue;
            audibleRanges = subtractRange(audibleRanges,
                { priorityClip.startSeconds,
                  priorityClip.startSeconds + priorityClip.lengthSeconds });
        }

        for (const auto range : audibleRanges)
        {
            segments.push_back({
                clipIndex,
                range.start,
                clip.sourceOffsetSeconds + range.start - clip.startSeconds,
                range.end - range.start
            });
        }
    }

    return segments;
}
}

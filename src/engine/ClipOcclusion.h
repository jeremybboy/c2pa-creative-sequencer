#pragma once

#include "project/Project.h"

#include <cstddef>
#include <vector>

namespace c2paseq
{
struct PlaybackClipSegment
{
    std::size_t clipIndex = 0;
    double startSeconds = 0.0;
    double sourceOffsetSeconds = 0.0;
    double lengthSeconds = 0.0;
};

// Clip order is arrangement priority: later clips occlude earlier clips on the
// same track. The saved ClipModels remain untouched so occluded audio is restored
// whenever a higher-priority clip is moved or deleted.
[[nodiscard]] std::vector<PlaybackClipSegment> buildPlaybackClipSegments(
    const std::vector<ClipModel>& clips);
}

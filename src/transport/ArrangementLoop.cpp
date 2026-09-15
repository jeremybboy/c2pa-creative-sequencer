#include "ArrangementLoop.h"

#include "transport/TransportFormatting.h"

#include <algorithm>

namespace c2paseq
{
ArrangementLoopRange resolveArrangementLoopRange(const Project& project,
                                                  const juce::String& selectedClipId)
{
    if (selectedClipId.isNotEmpty())
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                if (clip.id == selectedClipId && clip.lengthSeconds > 0.001)
                    return { clip.startSeconds, clip.startSeconds + clip.lengthSeconds };

    double arrangementEnd = 0.0;
    for (const auto& track : project.tracks)
        for (const auto& clip : track.clips)
            arrangementEnd = std::max(arrangementEnd,
                                      clip.startSeconds + clip.lengthSeconds);

    if (arrangementEnd <= 0.001)
    {
        const auto bpm = std::clamp(project.bpm, transport::minimumBpm,
                                    transport::maximumBpm);
        arrangementEnd = 4.0 * 60.0 / bpm;
    }
    return { 0.0, arrangementEnd };
}
}

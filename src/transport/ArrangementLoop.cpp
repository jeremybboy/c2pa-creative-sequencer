#include "ArrangementLoop.h"

#include "transport/TransportFormatting.h"

#include <algorithm>

namespace c2paseq
{
ArrangementLoopRange resolveArrangementLoopRange(const Project& project,
                                                  const juce::String& selectedClipId)
{
    const auto bpm = std::clamp(project.bpm, transport::minimumBpm,
                                transport::maximumBpm);
    const MusicalTimeConverter converter(bpm);
    if (selectedClipId.isNotEmpty())
        for (const auto& track : project.tracks)
        {
            for (const auto& clip : track.clips)
                if (clip.id == selectedClipId && clip.lengthSeconds > 0.001)
                    return { clip.startSeconds, clip.startSeconds + clip.lengthSeconds };
            for (const auto& clip : track.midiClips)
                if (clip.id == selectedClipId && clip.length.beats > 0.0)
                {
                    const auto start = converter.toSeconds(clip.start);
                    return { start, start + converter.toSeconds(clip.length) };
                }
        }

    double arrangementEnd = 0.0;
    for (const auto& track : project.tracks)
    {
        for (const auto& clip : track.clips)
            arrangementEnd = std::max(arrangementEnd,
                                      clip.startSeconds + clip.lengthSeconds);
        for (const auto& clip : track.midiClips)
            arrangementEnd = std::max(arrangementEnd,
                converter.toSeconds(clip.start) + converter.toSeconds(clip.length));
    }

    if (arrangementEnd <= 0.001)
    {
        arrangementEnd = 4.0 * 60.0 / bpm;
    }
    return { 0.0, arrangementEnd };
}
}

#pragma once

#include <algorithm>

namespace c2paseq
{
struct ArrangementTimeSelection
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    int firstTrack = 0;
    int lastTrack = 0;
    bool active = false;

    [[nodiscard]] bool isValid() const noexcept
    {
        return active && endSeconds > startSeconds + 0.001
            && firstTrack >= 0 && lastTrack >= firstTrack;
    }

    static ArrangementTimeSelection between(double firstTime, double secondTime,
                                             int firstTrackIndex, int lastTrackIndex)
    {
        ArrangementTimeSelection result;
        result.startSeconds = std::min(firstTime, secondTime);
        result.endSeconds = std::max(firstTime, secondTime);
        result.firstTrack = std::min(firstTrackIndex, lastTrackIndex);
        result.lastTrack = std::max(firstTrackIndex, lastTrackIndex);
        result.active = result.endSeconds > result.startSeconds + 0.001;
        return result;
    }
};
}

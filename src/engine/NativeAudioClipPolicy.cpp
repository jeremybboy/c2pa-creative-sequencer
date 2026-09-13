#include "NativeAudioClipPolicy.h"

namespace c2paseq
{
void configureNativeAudioClip(tracktion::engine::WaveAudioClip& clip,
                              double sourceLengthSeconds)
{
    if (clip.isLooping())
        clip.disableLooping();

    clip.setAutoTempo(false);
    clip.setAutoPitch(false);
    clip.setTimeStretchMode(tracktion::engine::TimeStretcher::disabled);
    clip.setSpeedRatio(1.0);
    clip.setSyncType(tracktion::engine::Clip::syncAbsolute);

    if (sourceLengthSeconds > 0.0)
    {
        const auto current = clip.getPosition();
        const auto duration = tracktion::TimeDuration::fromSeconds(sourceLengthSeconds);
        clip.setPosition({ { current.getStart(), current.getStart() + duration },
                           current.getOffset() });
    }
}
}

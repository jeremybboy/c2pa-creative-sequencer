#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace c2paseq
{
void configureNativeAudioClip(tracktion::engine::WaveAudioClip& clip,
                              double sourceLengthSeconds);
}

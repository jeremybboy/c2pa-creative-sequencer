#pragma once

#include <juce_core/juce_core.h>

namespace c2paseq
{
struct AudioInputChoice
{
    juce::String deviceName;
    int channelIndex = 0;
    juce::String channelName;
    juce::String label() const { return deviceName + " / " + channelName; }
};
}

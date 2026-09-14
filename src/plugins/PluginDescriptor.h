#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace c2paseq
{
struct PluginDescriptor
{
    juce::String identifier;
    juce::String name;
    juce::String vendor;
    juce::String version;
    juce::String format;
    juce::String category;
    juce::String fileOrIdentifier;
    int uniqueId = 0;
    int deprecatedUid = 0;
    bool isInstrument = false;

    [[nodiscard]] static PluginDescriptor fromJuce(const juce::PluginDescription&);
    [[nodiscard]] juce::PluginDescription toJuce() const;
};
}

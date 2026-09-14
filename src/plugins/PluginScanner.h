#pragma once

#include "PluginDescriptor.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace c2paseq
{
class PluginScanner final
{
public:
    explicit PluginScanner(juce::File cacheFile = defaultCacheFile());

    [[nodiscard]] const std::vector<PluginDescriptor>& cachedPlugins() const noexcept;
    [[nodiscard]] juce::Result scanVst3(const juce::FileSearchPath& paths = standardSearchPaths());
    [[nodiscard]] static juce::FileSearchPath standardSearchPaths();
    [[nodiscard]] static juce::File defaultCacheFile();

private:
    void loadCache();
    [[nodiscard]] juce::Result saveCache() const;
    void rebuildDescriptors();

    juce::File cache;
    juce::KnownPluginList knownPlugins;
    std::vector<PluginDescriptor> descriptors;
};
}

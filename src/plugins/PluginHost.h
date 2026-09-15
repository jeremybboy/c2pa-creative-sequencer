#pragma once

#include "PluginScanner.h"
#include "PluginWindow.h"

#include <map>
#include <memory>

namespace c2paseq
{
class ProjectEngine;
class TracktionAdapter;

class PluginHost final
{
public:
    PluginHost(TracktionAdapter&, ProjectEngine&,
               juce::File cacheFile = PluginScanner::defaultCacheFile(),
               bool showEditorWindows = true);
    ~PluginHost();

    [[nodiscard]] const std::vector<PluginDescriptor>& availablePlugins() const noexcept;
    [[nodiscard]] juce::Result scanVst3(const juce::FileSearchPath& paths = {});
    [[nodiscard]] juce::Result loadTrackPlugin(int trackIndex, const juce::String& identifier);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int trackIndex, bool bypassed);
    [[nodiscard]] juce::Result removeTrackPlugin(int trackIndex);
    [[nodiscard]] juce::Result openTrackPluginEditor(int trackIndex);

private:
    void registerCachedPlugins();
    void closeEditor(int trackIndex);
    void closeAllEditors();

    TracktionAdapter& tracktion;
    ProjectEngine& projectEngine;
    PluginScanner scanner;
    bool showEditorWindows = true;
    std::map<int, std::unique_ptr<PluginWindow>> windows;
};
}

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
    void closeEditorsForOfflineRender();

    [[nodiscard]] const std::vector<PluginDescriptor>& availablePlugins() const noexcept;
    [[nodiscard]] juce::Result scanVst3(const juce::FileSearchPath& paths = {});
    [[nodiscard]] juce::Result scanVst3Bundle(const juce::File& bundle);
    [[nodiscard]] juce::Result loadTrackPlugin(int trackIndex, const juce::String& identifier);
    [[nodiscard]] juce::Result setTrackPluginBypassed(int trackIndex, bool bypassed);
    [[nodiscard]] juce::Result removeTrackPlugin(int trackIndex);
    [[nodiscard]] juce::Result openTrackPluginEditor(
        int trackIndex,
        std::function<bool(const juce::KeyPress&)> keyHandler = {});

private:
    void registerCachedPlugins();
    void closeEditor(int trackIndex);
    void closeAllEditors(bool persistState);

    TracktionAdapter& tracktion;
    ProjectEngine& projectEngine;
    PluginScanner scanner;
    bool showEditorWindows = true;
    std::map<int, std::unique_ptr<PluginWindow>> windows;
};
}

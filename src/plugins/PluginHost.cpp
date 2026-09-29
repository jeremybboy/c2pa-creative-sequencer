#include "PluginHost.h"

#include "engine/ProjectEngine.h"
#include "engine/TracktionAdapter.h"

#include <algorithm>

namespace c2paseq
{
PluginHost::PluginHost(TracktionAdapter& adapter, ProjectEngine& project,
                       juce::File cacheFile, bool shouldShowEditorWindows)
    : tracktion(adapter), projectEngine(project), scanner(std::move(cacheFile)),
      showEditorWindows(shouldShowEditorWindows)
{
    registerCachedPlugins();
    tracktion.setBeforeEditReplacement([this]
    {
        closeAllEditors(false);
    });
}

PluginHost::~PluginHost()
{
    tracktion.setBeforeEditReplacement({});
    closeAllEditors(true);
}

const std::vector<PluginDescriptor>& PluginHost::availablePlugins() const noexcept
{
    return scanner.cachedPlugins();
}

juce::Result PluginHost::scanVst3(const juce::FileSearchPath& paths)
{
    const auto searchPaths = paths.getNumPaths() > 0 ? paths
                                                     : PluginScanner::standardSearchPaths();
    if (auto result = scanner.scanVst3(searchPaths); result.failed())
        return result;
    registerCachedPlugins();
    return juce::Result::ok();
}

juce::Result PluginHost::scanVst3Bundle(const juce::File& bundle)
{
    if (auto result = scanner.scanVst3Bundle(bundle); result.failed())
        return result;
    registerCachedPlugins();
    return juce::Result::ok();
}

juce::Result PluginHost::loadTrackPlugin(int trackIndex, const juce::String& identifier)
{
    const auto& plugins = scanner.cachedPlugins();
    const auto found = std::find_if(plugins.begin(), plugins.end(), [&](const auto& plugin)
    {
        return plugin.identifier == identifier;
    });
    if (found == plugins.end())
        return juce::Result::fail("VST3 is not in the scanned plug-in cache");
    closeEditor(trackIndex);
    return projectEngine.setTrackPlugin(trackIndex, *found);
}

juce::Result PluginHost::setTrackPluginBypassed(int trackIndex, bool bypassed)
{
    return projectEngine.setTrackPluginBypassed(trackIndex, bypassed);
}

juce::Result PluginHost::removeTrackPlugin(int trackIndex)
{
    closeEditor(trackIndex);
    return projectEngine.removeTrackPlugin(trackIndex);
}

juce::Result PluginHost::openTrackPluginEditor(
    int trackIndex,
    std::function<bool(const juce::KeyPress&)> keyHandler)
{
    closeEditor(trackIndex);
    auto* instance = tracktion.trackPluginInstance(trackIndex);
    if (instance == nullptr)
        return juce::Result::fail("The track VST3 is missing or failed to load");

    auto window = std::make_unique<PluginWindow>(*instance, [this, trackIndex]
    {
        closeEditor(trackIndex);
    }, std::move(keyHandler), showEditorWindows);
    windows[trackIndex] = std::move(window);
    return juce::Result::ok();
}

void PluginHost::registerCachedPlugins()
{
    for (const auto& descriptor : scanner.cachedPlugins())
        tracktion.registerPluginDescription(descriptor.toJuce());
}

void PluginHost::closeEditor(int trackIndex)
{
    if (windows.erase(trackIndex) > 0)
        (void) projectEngine.saveProject();
}

void PluginHost::closeAllEditors(bool persistState)
{
    if (persistState && ! windows.empty())
        (void) projectEngine.saveProject();
    windows.clear();
}
}

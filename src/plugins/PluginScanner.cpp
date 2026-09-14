#include "PluginScanner.h"

#include <algorithm>

namespace c2paseq
{
PluginScanner::PluginScanner(juce::File cacheFile) : cache(std::move(cacheFile))
{
    loadCache();
}

const std::vector<PluginDescriptor>& PluginScanner::cachedPlugins() const noexcept
{
    return descriptors;
}

juce::Result PluginScanner::scanVst3(const juce::FileSearchPath& paths)
{
    juce::VST3PluginFormat format;
    const auto candidates = format.searchPathsForPlugins(paths, true, false);
    knownPlugins.clear();

    for (const auto& candidate : candidates)
    {
        juce::OwnedArray<juce::PluginDescription> found;
        knownPlugins.scanAndAddFile(candidate, false, found, format);
    }

    knownPlugins.sort(juce::KnownPluginList::sortByManufacturer, true);
    rebuildDescriptors();
    return saveCache();
}

juce::FileSearchPath PluginScanner::standardSearchPaths()
{
    juce::FileSearchPath paths;
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/VST3"));
    paths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    return paths;
}

juce::File PluginScanner::defaultCacheFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("vst3-cache.xml");
}

void PluginScanner::loadCache()
{
    if (auto xml = juce::parseXML(cache))
        knownPlugins.recreateFromXml(*xml);
    rebuildDescriptors();
}

juce::Result PluginScanner::saveCache() const
{
    if (! cache.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not create the VST3 cache directory");
    const auto xml = knownPlugins.createXml();
    if (xml == nullptr || ! xml->writeTo(cache))
        return juce::Result::fail("Could not save the VST3 cache");
    return juce::Result::ok();
}

void PluginScanner::rebuildDescriptors()
{
    descriptors.clear();
    for (const auto& type : knownPlugins.getTypes())
        if (type.pluginFormatName == "VST3")
            descriptors.push_back(PluginDescriptor::fromJuce(type));

    std::sort(descriptors.begin(), descriptors.end(), [](const auto& a, const auto& b)
    {
        const auto vendorOrder = a.vendor.compareIgnoreCase(b.vendor);
        return vendorOrder != 0 ? vendorOrder < 0 : a.name.compareIgnoreCase(b.name) < 0;
    });
}
}

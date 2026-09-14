#include "PluginDescriptor.h"

namespace c2paseq
{
PluginDescriptor PluginDescriptor::fromJuce(const juce::PluginDescription& value)
{
    return {
        value.createIdentifierString(), value.name, value.manufacturerName,
        value.version, value.pluginFormatName, value.category,
        value.fileOrIdentifier, value.uniqueId, value.deprecatedUid,
        value.isInstrument
    };
}

juce::PluginDescription PluginDescriptor::toJuce() const
{
    juce::PluginDescription value;
    value.name = name;
    value.descriptiveName = name;
    value.manufacturerName = vendor;
    value.version = version;
    value.pluginFormatName = format;
    value.category = category;
    value.fileOrIdentifier = fileOrIdentifier;
    value.uniqueId = uniqueId;
    value.deprecatedUid = deprecatedUid;
    value.isInstrument = isInstrument;
    return value;
}
}

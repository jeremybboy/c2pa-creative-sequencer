#include "TrackLevelMeterPlugin.h"

namespace c2paseq
{
const char* TrackLevelMeterPlugin::xmlTypeName = "c2paTrackMeter";

TrackLevelMeterPlugin::TrackLevelMeterPlugin(tracktion::engine::PluginCreationInfo info)
    : Plugin(info)
{
}

TrackLevelMeterPlugin::~TrackLevelMeterPlugin()
{
    notifyListenersOfDeletion();
}

juce::ValueTree TrackLevelMeterPlugin::create()
{
    return tracktion::engine::createValueTree(tracktion::engine::IDs::PLUGIN,
                                               tracktion::engine::IDs::type,
                                               xmlTypeName);
}

void TrackLevelMeterPlugin::initialise(
    const tracktion::engine::PluginInitialisationInfo&)
{
    accumulator.reset();
}

void TrackLevelMeterPlugin::deinitialise()
{
    accumulator.reset();
}

void TrackLevelMeterPlugin::reset()
{
    accumulator.reset();
}

void TrackLevelMeterPlugin::applyToBuffer(
    const tracktion::engine::PluginRenderContext& context)
{
    if (context.destBuffer != nullptr && context.isPlaying && ! context.isRendering)
        accumulator.process(*context.destBuffer, context.bufferStartSample,
                            context.bufferNumSamples);
}

TrackLevelSnapshot TrackLevelMeterPlugin::consumePeaks() noexcept
{
    return accumulator.consume();
}
}

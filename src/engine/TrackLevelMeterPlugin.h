#pragma once

#include "TrackLevelMeter.h"

#include <tracktion_engine/tracktion_engine.h>

namespace c2paseq
{
class TrackLevelMeterPlugin final : public tracktion::engine::Plugin
{
public:
    explicit TrackLevelMeterPlugin(tracktion::engine::PluginCreationInfo);
    ~TrackLevelMeterPlugin() override;

    static const char* getPluginName() { return "C2PA Track Meter"; }
    static const char* xmlTypeName;
    static juce::ValueTree create();

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    void initialise(const tracktion::engine::PluginInitialisationInfo&) override;
    void deinitialise() override;
    void reset() override;
    void applyToBuffer(const tracktion::engine::PluginRenderContext&) override;
    int getNumOutputChannelsGivenInputs(int inputs) override { return inputs; }
    bool producesAudioWhenNoAudioInput() override { return false; }
    bool shouldMeasureCpuUsage() const noexcept override { return false; }
    juce::String getSelectableDescription() override { return getPluginName(); }

    [[nodiscard]] TrackLevelSnapshot consumePeaks() noexcept;

private:
    TrackLevelAccumulator accumulator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackLevelMeterPlugin)
};
}

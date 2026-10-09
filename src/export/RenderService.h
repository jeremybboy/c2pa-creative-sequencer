#pragma once

#include "provenance/ProvenanceModel.h"
#include "project/Project.h"
#include "project/ProjectPaths.h"

namespace c2paseq
{
class TracktionAdapter;

struct RenderPlan
{
    double endSeconds = 0.0;
    std::vector<ContributingIngredient> ingredients;
};

struct MidiStemPlan
{
    int trackIndex = -1;
    juce::String projectId;
    juce::String trackId;
    juce::String trackName;
    juce::String sourceSignature;
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    StemProvenanceDescriptor descriptor;
};

class RenderService final
{
public:
    [[nodiscard]] static juce::Result createMidiStemPlan(const Project&, int trackIndex,
                                                         MidiStemPlan&);
    [[nodiscard]] static juce::Result createPlan(const Project& project,
                                                  const ProjectPaths& paths,
                                                  RenderPlan& plan);
    [[nodiscard]] static juce::Result render(TracktionAdapter& tracktion,
                                              const RenderPlan& plan,
                                              const juce::File& destination);
};
}

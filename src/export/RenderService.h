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

class RenderService final
{
public:
    [[nodiscard]] static juce::Result createPlan(const Project& project,
                                                  const ProjectPaths& paths,
                                                  RenderPlan& plan);
    [[nodiscard]] static juce::Result render(TracktionAdapter& tracktion,
                                              const RenderPlan& plan,
                                              const juce::File& destination);
};
}

#pragma once

#include "ExportResult.h"
#include "RenderService.h"

namespace c2paseq
{
class ProvenanceService;
class TracktionAdapter;

class ExportController final
{
public:
    ExportController(TracktionAdapter& tracktion, ProvenanceService& provenance);

    [[nodiscard]] ExportResult exportMix(const Project& project,
                                          const ProjectPaths& paths,
                                          const juce::File& destination);

private:
    TracktionAdapter& tracktion;
    ProvenanceService& provenance;
};
}

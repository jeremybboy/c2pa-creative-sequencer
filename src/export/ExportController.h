#pragma once

#include "ExportResult.h"
#include "RenderService.h"

namespace c2paseq
{
class ProvenanceService;
class TracktionAdapter;
class WatermarkService;
class SoftBindingStore;

class ExportController final
{
public:
    ExportController(TracktionAdapter& tracktion, ProvenanceService& provenance,
                     WatermarkService* watermark = nullptr,
                     SoftBindingStore* store = nullptr,
                     bool softBindingEnabled = false);

    [[nodiscard]] ExportResult exportMix(const Project& project,
                                          const ProjectPaths& paths,
                                          const juce::File& destination);

private:
    TracktionAdapter& tracktion;
    ProvenanceService& provenance;
    WatermarkService* watermarkService = nullptr;
    SoftBindingStore* recoveryStore = nullptr;
    bool useSoftBinding = false;
};
}

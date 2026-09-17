#pragma once

#include "ExportResult.h"
#include "RenderService.h"

namespace c2paseq
{
class ProvenanceService;
class TracktionAdapter;
class WatermarkService;
class SoftBindingOutbox;

class ExportController final
{
public:
    ExportController(TracktionAdapter& tracktion, ProvenanceService& provenance,
                     WatermarkService* watermark = nullptr,
                     SoftBindingOutbox* outbox = nullptr,
                     bool softBindingEnabled = false,
                     ExportProgressCallback progress = {},
                     ExportCancellationCheck shouldCancel = {});

    [[nodiscard]] ExportResult exportMix(const Project& project,
                                          const ProjectPaths& paths,
                                          const juce::File& destination);

private:
    TracktionAdapter& tracktion;
    ProvenanceService& provenance;
    WatermarkService* watermarkService = nullptr;
    SoftBindingOutbox* publicationOutbox = nullptr;
    bool useSoftBinding = false;
    ExportProgressCallback progressCallback;
    ExportCancellationCheck cancellationCheck;
};
}

#pragma once

#include "ExportResult.h"
#include "RenderService.h"

namespace c2paseq
{
class ProvenanceService;
class TracktionAdapter;
class WatermarkService;
class SoftBindingOutbox;
class FingerprintService;

class ExportController final
{
public:
    ExportController(TracktionAdapter& tracktion, ProvenanceService& provenance,
                     WatermarkService* watermark = nullptr,
                     SoftBindingOutbox* outbox = nullptr,
                     bool softBindingEnabled = false,
                     FingerprintService* fingerprint = nullptr,
                     bool fingerprintEnabled = false,
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
    FingerprintService* fingerprintService = nullptr;
    bool useFingerprint = false;
    ExportProgressCallback progressCallback;
    ExportCancellationCheck cancellationCheck;
};
}

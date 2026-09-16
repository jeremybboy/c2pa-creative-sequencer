#include "SoftBindingRecoveryService.h"

namespace c2paseq
{
SoftBindingRecoveryService::SoftBindingRecoveryService(WatermarkService& watermarkService,
                                                       SoftBindingStore& recoveryStore,
                                                       ProvenanceService& provenanceService)
    : watermark(watermarkService), store(recoveryStore), provenance(provenanceService)
{
}

juce::Result SoftBindingRecoveryService::recover(const juce::File& asset,
                                                  IngredientInfo& recovered)
{
    const auto embedded = provenance.inspect(asset);
    if (embedded.c2paPresent)
    {
        recovered = embedded;
        return juce::Result::ok();
    }
    if (embedded.status != ProvenanceStatus::noCredentials)
        return juce::Result::fail("Embedded credential inspection did not permit recovery");
    SoftBindingPayload payload;
    if (const auto decoded = watermark.decode(asset, payload); decoded.failed())
        return juce::Result::fail("WavMark recovery failed: " + decoded.getErrorMessage());
    StoredSoftBinding stored;
    if (const auto resolved = store.resolve(payload, stored); resolved.failed())
        return resolved;
    auto candidate = provenance.inspectRecoveredManifest(asset, stored.manifestBytes);
    if (! candidate.c2paPresent)
        return juce::Result::fail("Stored recovery file is not an inspectable signed manifest");
    if (! ProvenanceService::hasMatchingSoftBinding(candidate, payload))
        return juce::Result::fail(
            "Recovered manifest does not contain the decoded WavMark algorithm and payload");
    candidate.retrievalMode = ProvenanceRetrievalMode::recoveredSoftBinding;
    candidate.softBindingAlgorithm = SoftBindingStore::algorithm;
    candidate.softBindingPayloadHex = payload.toHex();
    recovered = std::move(candidate);
    return juce::Result::ok();
}
}

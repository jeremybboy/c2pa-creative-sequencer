#include "ExportController.h"

#include "provenance/ProvenanceService.h"
#include "watermark/SoftBindingStore.h"
#include "watermark/WatermarkService.h"

#include <cmath>

namespace c2paseq
{
ExportController::ExportController(TracktionAdapter& adapter, ProvenanceService& service,
                                   WatermarkService* watermark,
                                   SoftBindingStore* store,
                                   bool softBindingEnabled)
    : tracktion(adapter), provenance(service), watermarkService(watermark),
      recoveryStore(store), useSoftBinding(softBindingEnabled)
{
}

ExportResult ExportController::exportMix(const Project& project,
                                         const ProjectPaths& paths,
                                         const juce::File& destination)
{
    ExportResult output;
    output.outputFile = destination;
    output.softBindingEnabled = useSoftBinding;
    RenderPlan plan;
    output.result = RenderService::createPlan(project, paths, plan);
    if (output.result.failed())
        return output;
    output.ingredients = plan.ingredients;

    if (! provenance.signingConfigured())
    {
        output.stage = ExportStage::signingConfiguration;
        output.result = juce::Result::fail(provenance.signingConfigurationError());
        return output;
    }

    juce::TemporaryFile unsignedRender(destination);
    output.stage = ExportStage::audioRender;
    output.result = RenderService::render(tracktion, plan, unsignedRender.getFile());
    if (output.result.failed())
        return output;
    output.audioRendered = true;

    juce::TemporaryFile watermarkedRender(destination);
    juce::File signingInput = unsignedRender.getFile();
    std::optional<SoftBindingClaim> softBinding;
    if (useSoftBinding)
    {
        if (watermarkService == nullptr || recoveryStore == nullptr
            || ! watermarkService->isAvailable())
        {
            output.stage = ExportStage::watermarkEmbedding;
            output.result = juce::Result::fail(watermarkService != nullptr
                ? watermarkService->statusDescription()
                : "WavMark soft binding is not configured");
            return output;
        }
        SoftBindingPayload payload;
        if (const auto allocated = recoveryStore->allocatePayload(payload); allocated.failed())
        {
            output.stage = ExportStage::watermarkEmbedding;
            output.result = allocated;
            return output;
        }
        WatermarkEmbedResult details;
        output.stage = ExportStage::watermarkEmbedding;
        output.result = watermarkService->embed(unsignedRender.getFile(),
            watermarkedRender.getFile(), payload, details);
        if (output.result.failed()) return output;
        output.stage = ExportStage::watermarkVerification;
        SoftBindingPayload decoded;
        output.result = watermarkService->decode(watermarkedRender.getFile(), decoded);
        if (output.result.failed()) return output;
        if (decoded != payload)
        {
            output.result = juce::Result::fail(
                "WavMark verification decoded a different soft-binding payload");
            return output;
        }
        output.watermarkVerified = true;
        output.watermarkSnrDb = details.snrDb;
        output.softBindingPayloadHex = payload.toHex();
        softBinding = SoftBindingClaim { payload, 0,
            static_cast<std::uint64_t>(std::llround(plan.endSeconds * 1000.0)) };
        signingInput = watermarkedRender.getFile();
    }

    juce::TemporaryFile signedRender(destination);
    output.stage = ExportStage::signingAndEmbedding;
    std::vector<std::uint8_t> manifestStore;
    output.result = provenance.signWav(signingInput, signedRender.getFile(),
                                       plan.ingredients, destination.getFileName(),
                                       output.outputProvenance, softBinding,
                                       useSoftBinding ? &manifestStore : nullptr);
    if (output.result.failed())
        return output;
    output.credentialsAttached = true;
    output.credentialsValidated = output.outputProvenance.assetIntact;
    output.externallyTrusted = output.outputProvenance.status == ProvenanceStatus::valid;

    if (useSoftBinding)
    {
        output.stage = ExportStage::recoveryStorePersistence;
        output.result = recoveryStore->persist(softBinding->payload, manifestStore,
            destination.getFileName(), output.softBindingManifestId);
        if (output.result.failed()) return output;
    }

    if (! signedRender.overwriteTargetFileWithTemporary())
    {
        output.stage = ExportStage::fileCommit;
        output.result = juce::Result::fail("Could not commit signed WAV export");
        return output;
    }
    output.outputProvenance = provenance.inspect(destination);
    if (! output.outputProvenance.c2paPresent || ! output.outputProvenance.assetIntact)
    {
        destination.deleteFile();
        output.stage = ExportStage::finalValidation;
        output.result = juce::Result::fail("Committed export failed final C2PA validation");
        return output;
    }

    output.stage = ExportStage::complete;
    output.result = juce::Result::ok();
    return output;
}
}

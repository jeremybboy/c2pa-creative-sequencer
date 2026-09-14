#include "ExportController.h"

#include "provenance/ProvenanceService.h"

namespace c2paseq
{
ExportController::ExportController(TracktionAdapter& adapter, ProvenanceService& service)
    : tracktion(adapter), provenance(service)
{
}

ExportResult ExportController::exportMix(const Project& project,
                                         const ProjectPaths& paths,
                                         const juce::File& destination)
{
    ExportResult output;
    output.outputFile = destination;
    RenderPlan plan;
    output.result = RenderService::createPlan(project, paths, plan);
    if (output.result.failed())
        return output;
    output.ingredients = plan.ingredients;

    juce::TemporaryFile unsignedRender(destination);
    output.stage = ExportStage::audioRender;
    output.result = RenderService::render(tracktion, plan, unsignedRender.getFile());
    if (output.result.failed())
        return output;
    output.audioRendered = true;

    if (! provenance.signingConfigured())
    {
        output.stage = ExportStage::signingConfiguration;
        if (! unsignedRender.overwriteTargetFileWithTemporary())
        {
            output.stage = ExportStage::fileCommit;
            output.result = juce::Result::fail("Could not commit unsigned WAV export");
            return output;
        }
        output.unsignedBecauseNotConfigured = true;
        output.result = juce::Result::ok();
        return output;
    }

    juce::TemporaryFile signedRender(destination);
    output.stage = ExportStage::signingAndEmbedding;
    output.result = provenance.signWav(unsignedRender.getFile(), signedRender.getFile(),
                                       plan.ingredients, destination.getFileName(),
                                       output.outputProvenance);
    if (output.result.failed())
        return output;
    output.credentialsAttached = true;
    output.credentialsValidated = output.outputProvenance.assetIntact;
    output.externallyTrusted = output.outputProvenance.status == ProvenanceStatus::valid;

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

#include "ExportController.h"

#include "app/AppInfo.h"
#include "provenance/ProvenanceService.h"
#include "watermark/SoftBindingOutbox.h"
#include "watermark/WatermarkService.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

namespace c2paseq
{
namespace
{
struct AudioProperties
{
    double sampleRate = 0.0;
    int channels = 0;
    int bitsPerSample = 0;
    juce::int64 frames = 0;
};

juce::Result readAudioProperties(const juce::File& file, AudioProperties& properties)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (reader == nullptr)
        return juce::Result::fail("Export stage produced an unreadable WAV");
    properties = { reader->sampleRate, static_cast<int>(reader->numChannels),
                   static_cast<int>(reader->bitsPerSample), reader->lengthInSamples };
    return juce::Result::ok();
}
}

ExportController::ExportController(TracktionAdapter& adapter, ProvenanceService& service,
                                   WatermarkService* watermark,
                                   SoftBindingOutbox* outbox,
                                   bool softBindingEnabled,
                                   ExportProgressCallback progress,
                                   ExportCancellationCheck shouldCancel)
    : tracktion(adapter), provenance(service), watermarkService(watermark),
      publicationOutbox(outbox), useSoftBinding(softBindingEnabled),
      progressCallback(std::move(progress)), cancellationCheck(std::move(shouldCancel))
{
}

ExportResult ExportController::exportMix(const Project& project,
                                         const ProjectPaths& paths,
                                         const juce::File& destination)
{
    ExportResult output;
    const auto totalStart = juce::Time::getMillisecondCounterHiRes();
    output.outputFile = destination;
    output.softBindingEnabled = useSoftBinding;
    const auto setStage = [&](ExportStage stage)
    {
        output.stage = stage;
        if (progressCallback) progressCallback(stage);
    };
    const auto cancelled = [&]
    {
        if (! cancellationCheck || ! cancellationCheck()) return false;
        setStage(ExportStage::cancelled);
        output.result = juce::Result::fail("Export cancelled");
        output.totalSeconds = (juce::Time::getMillisecondCounterHiRes() - totalStart) / 1000.0;
        return true;
    };

    setStage(ExportStage::planning);
    RenderPlan plan;
    output.result = RenderService::createPlan(project, paths, plan);
    if (output.result.failed() || cancelled()) return output;
    output.ingredients = plan.ingredients;

    if (! provenance.signingConfigured())
    {
        setStage(ExportStage::signingConfiguration);
        output.result = juce::Result::fail(provenance.signingConfigurationError());
        return output;
    }

    juce::TemporaryFile unsignedRender(destination);
    setStage(ExportStage::audioRender);
    const auto renderStart = juce::Time::getMillisecondCounterHiRes();
    output.result = RenderService::render(tracktion, plan, unsignedRender.getFile());
    output.renderSeconds = (juce::Time::getMillisecondCounterHiRes() - renderStart) / 1000.0;
    if (output.result.failed() || cancelled()) return output;
    output.audioRendered = true;

    juce::TemporaryFile watermarkedRender(destination);
    juce::File signingInput = unsignedRender.getFile();
    std::optional<SoftBindingClaim> softBinding;
    if (useSoftBinding)
    {
        if (watermarkService == nullptr || publicationOutbox == nullptr
            || ! watermarkService->isAvailable())
        {
            setStage(ExportStage::watermarkEmbedding);
            output.result = juce::Result::fail(watermarkService != nullptr
                ? watermarkService->statusDescription()
                : "AudioWMark soft binding is not configured");
            return output;
        }
        SoftBindingPayload payload;
        if (const auto allocated = publicationOutbox->allocatePayload(payload); allocated.failed())
        {
            setStage(ExportStage::watermarkEmbedding);
            output.result = allocated;
            return output;
        }
        AudioProperties before;
        if (const auto inspected = readAudioProperties(unsignedRender.getFile(), before);
            inspected.failed())
        {
            output.result = inspected;
            return output;
        }
        WatermarkEmbedResult details;
        setStage(ExportStage::watermarkEmbedding);
        output.result = watermarkService->embed(unsignedRender.getFile(),
            watermarkedRender.getFile(), payload, details, cancellationCheck);
        output.watermarkEmbedSeconds = details.elapsedSeconds;
        if (cancellationCheck && cancellationCheck())
        {
            cancelled();
            return output;
        }
        if (output.result.failed()) return output;
        AudioProperties after;
        if (const auto inspected = readAudioProperties(watermarkedRender.getFile(), after);
            inspected.failed())
        {
            output.result = inspected;
            return output;
        }
        if (before.channels != 2 || after.channels != 2
            || before.bitsPerSample != 24 || after.bitsPerSample != 24
            || std::abs(before.sampleRate - after.sampleRate) > 0.01
            || before.frames != after.frames)
        {
            output.result = juce::Result::fail(
                "AudioWMark output did not preserve stereo, sample rate, duration, and 24-bit PCM");
            return output;
        }
        output.audioPropertiesVerified = true;
        output.softBindingPayloadHex = payload.toHex();
        softBinding = SoftBindingClaim { payload, 0,
            static_cast<std::uint64_t>(std::llround(plan.endSeconds * 1000.0)) };
        signingInput = watermarkedRender.getFile();
    }

    juce::TemporaryFile signedRender(destination);
    setStage(ExportStage::creatingContentCredentials);
    if (cancelled()) return output;
    setStage(ExportStage::signingAndEmbedding);
    const auto signingStart = juce::Time::getMillisecondCounterHiRes();
    std::vector<std::uint8_t> manifestStore;
    output.result = provenance.signWav(signingInput, signedRender.getFile(),
        plan.ingredients, destination.getFileName(), output.outputProvenance,
        softBinding, useSoftBinding ? &manifestStore : nullptr);
    output.signingAndValidationSeconds =
        (juce::Time::getMillisecondCounterHiRes() - signingStart) / 1000.0;
    if (output.result.failed() || cancelled()) return output;
    output.credentialsAttached = true;
    output.credentialsValidated = output.outputProvenance.assetIntact;
    output.externallyTrusted = output.outputProvenance.status == ProvenanceStatus::valid;
    setStage(ExportStage::finalValidation);

    if (useSoftBinding)
    {
        setStage(ExportStage::publicationOutbox);
        const auto publicationStart = juce::Time::getMillisecondCounterHiRes();
        SoftBindingPublication publication;
        output.result = publicationOutbox->publish(softBinding->payload, manifestStore,
            destination.getFileName(), juce::String(appInfo::name.data()),
            output.outputProvenance.activeManifest, publication);
        output.publicationSeconds =
            (juce::Time::getMillisecondCounterHiRes() - publicationStart) / 1000.0;
        if (output.result.failed()) return output;
        output.softBindingManifestId = publication.activeManifest;
        output.publicationPackage = publication.packageDirectory;
    }

    setStage(ExportStage::fileCommit);
    if (! signedRender.overwriteTargetFileWithTemporary())
    {
        if (output.publicationPackage.isDirectory())
            output.publicationPackage.deleteRecursively();
        output.result = juce::Result::fail("Could not commit signed WAV export");
        return output;
    }
    output.outputProvenance = provenance.inspect(destination);
    if (! output.outputProvenance.c2paPresent || ! output.outputProvenance.assetIntact)
    {
        destination.deleteFile();
        if (output.publicationPackage.isDirectory())
            output.publicationPackage.deleteRecursively();
        setStage(ExportStage::finalValidation);
        output.result = juce::Result::fail("Committed export failed final C2PA validation");
        return output;
    }

    setStage(ExportStage::complete);
    output.totalSeconds = (juce::Time::getMillisecondCounterHiRes() - totalStart) / 1000.0;
    output.result = juce::Result::ok();
    return output;
}
}

#include "ExportController.h"

#include "app/AppInfo.h"
#include "fingerprint/FingerprintService.h"
#include "engine/TracktionAdapter.h"
#include "provenance/ProvenanceService.h"
#include "watermark/SoftBindingOutbox.h"
#include "watermark/WatermarkService.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <cmath>

namespace c2paseq
{
namespace
{
std::vector<std::uint8_t> bytesFromHex(const juce::String& text)
{
    std::vector<std::uint8_t> bytes;
    if (text.length() % 2 != 0) return bytes;
    bytes.reserve(static_cast<std::size_t>(text.length() / 2));
    for (int index = 0; index < text.length(); index += 2)
    {
        const auto value = text.substring(index, index + 2).getHexValue32();
        bytes.push_back(static_cast<std::uint8_t>(value));
    }
    return bytes;
}

struct AudioProperties
{
    double sampleRate = 0.0;
    int channels = 0;
    int bitsPerSample = 0;
    juce::int64 frames = 0;
};

struct ExportWorkspace
{
    explicit ExportWorkspace(TracktionAdapter& adapter) : tracktion(adapter) {}
    ~ExportWorkspace()
    {
        if (active) tracktion::engine::callBlocking([this] { tracktion.finishOfflineExport(); });
        directory.deleteRecursively();
    }
    TracktionAdapter& tracktion;
    bool active = false;
    juce::File directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("c2paseq-midi-export-" + juce::Uuid().toString());
};

struct IsolatedRenderScope
{
    TracktionAdapter& tracktion;
    ~IsolatedRenderScope()
    { tracktion::engine::callBlocking([this] { tracktion.finishExclusiveTrackRender(); }); }
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
                                   FingerprintService* fingerprint,
                                   bool fingerprintEnabled,
                                   ExportProgressCallback progress,
                                   ExportCancellationCheck shouldCancel)
    : tracktion(adapter), provenance(service), watermarkService(watermark),
      publicationOutbox(outbox), useSoftBinding(softBindingEnabled),
      fingerprintService(fingerprint), useFingerprint(fingerprintEnabled),
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
    output.fingerprintEnabled = useFingerprint;
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
    if (! destination.hasFileExtension("wav") || destination.isDirectory())
    {
        output.result = juce::Result::fail("Export destination must be a WAV file");
        return output;
    }

    if (! provenance.signingConfigured())
    {
        setStage(ExportStage::signingConfiguration);
        output.result = juce::Result::fail(provenance.signingConfigurationError());
        return output;
    }

    // These signed WAVs are actual inputs to the mix, not decorative credentials
    // attached to a separately rerendered instrument performance.
    ExportWorkspace midiWorkspace(tracktion);
    // Audio-only worker exports need the same owner-thread graph protection.
    tracktion::engine::callBlocking([&]
    {
        output.result = tracktion.beginOfflineExport();
        midiWorkspace.active = output.result.wasOk();
    });
    if (output.result.failed()) return output;
    if (! plan.midiStems.empty())
    {
        output.result = midiWorkspace.directory.createDirectory();
        if (output.result.failed()) return output;
        for (const auto& stem : plan.midiStems)
        {
            if (cancelled()) return output;
            const auto unsignedStem = midiWorkspace.directory.getChildFile(
                juce::String(stem.trackIndex) + "-unsigned.wav");
            const auto signedStem = midiWorkspace.directory.getChildFile(
                juce::String(stem.trackIndex) + "-signed.wav");
            setStage(ExportStage::midiStemRender);
            tracktion::engine::callBlocking([&]
            { output.result = tracktion.beginExclusiveTrackRender(stem.trackIndex, false); });
            if (output.result.failed()) return output;
            {
                IsolatedRenderScope renderScope { tracktion };
                output.result = tracktion.renderTrackWav(unsignedStem, stem.trackIndex,
                    stem.endSeconds, stem.startSeconds, false);
            }
            if (output.result.failed() || cancelled()) return output;
            setStage(ExportStage::midiStemSigning);
            IngredientInfo validation;
            output.result = provenance.signStemWav(unsignedStem, signedStem,
                                                    stem.descriptor, validation);
            if (output.result.failed() || cancelled()) return output;
            if (! validation.c2paPresent || ! validation.assetIntact)
            {
                output.result = juce::Result::fail("The MIDI export stem failed C2PA integrity validation");
                return output;
            }
            plan.ingredients.push_back({ "midi-export-" + stem.trackId, stem.descriptor.title,
                juce::SHA256(signedStem).toHexString(), signedStem, validation });
            ++output.midiStemsSigned;
        }
        // All stems derive independently from the original instrument graph.
        const auto firstStem = plan.ingredients.size() - plan.midiStems.size();
        for (std::size_t index = 0; index < plan.midiStems.size(); ++index)
        {
            const auto& stem = plan.midiStems[index];
            tracktion::engine::callBlocking([&]
            { output.result = tracktion.substituteMidiExportStem(stem.trackIndex,
                plan.ingredients[firstStem + index].file, stem.startSeconds,
                stem.endSeconds - stem.startSeconds); });
            if (output.result.failed() || cancelled()) return output;
        }
        output.ingredients = plan.ingredients;
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
    std::optional<SoftBindingPayload> watermarkPayload;
    std::vector<SoftBindingClaim> softBindings;
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
        watermarkPayload = payload;
        softBindings.push_back(makeAudioWMarkClaim(payload,
            static_cast<std::uint64_t>(std::llround(plan.endSeconds * 1000.0))));
        signingInput = watermarkedRender.getFile();
    }

    juce::TemporaryFile fingerprintArtifact(
        destination.getSiblingFile(destination.getFileNameWithoutExtension() + ".afpt"));
    std::optional<FingerprintRegistration> fingerprintRegistration;
    if (useFingerprint)
    {
        if (fingerprintService == nullptr || publicationOutbox == nullptr
            || ! fingerprintService->isAvailable())
        {
            setStage(ExportStage::fingerprintComputation);
            output.result = juce::Result::fail(fingerprintService != nullptr
                ? fingerprintService->statusDescription()
                : "audfprint soft binding is not configured");
            return output;
        }
        setStage(ExportStage::fingerprintComputation);
        FingerprintRegistration registration;
        output.result = fingerprintService->compute(signingInput,
            fingerprintArtifact.getFile(), registration, cancellationCheck);
        output.fingerprintSeconds = registration.elapsedSeconds;
        if (cancelled() || output.result.failed()) return output;
        const auto value = bytesFromHex(registration.valueHex);
        if (value.size() != 32)
        {
            output.result = juce::Result::fail(
                "audfprint registration did not produce a SHA-256 identifier");
            return output;
        }
        output.fingerprintValueHex = registration.valueHex;
        softBindings.push_back({ juce::String(audfprintAlgorithm.data()),
            SoftBindingType::fingerprint, value,
            SoftBindingScope { 0, static_cast<std::uint64_t>(
                std::llround(plan.endSeconds * 1000.0)) } });
        fingerprintRegistration = registration;
    }

    juce::TemporaryFile signedRender(destination);
    setStage(ExportStage::creatingContentCredentials);
    if (cancelled()) return output;
    setStage(ExportStage::signingAndEmbedding);
    const auto signingStart = juce::Time::getMillisecondCounterHiRes();
    std::vector<std::uint8_t> manifestStore;
    output.result = provenance.signWav(signingInput, signedRender.getFile(),
        plan.ingredients, destination.getFileName(), output.outputProvenance,
        softBindings, ! softBindings.empty() ? &manifestStore : nullptr);
    output.signingAndValidationSeconds =
        (juce::Time::getMillisecondCounterHiRes() - signingStart) / 1000.0;
    if (output.result.failed() || cancelled()) return output;
    output.credentialsAttached = true;
    output.credentialsValidated = output.outputProvenance.assetIntact;
    output.externallyTrusted = output.outputProvenance.status == ProvenanceStatus::valid;
    setStage(ExportStage::finalValidation);

    if (! softBindings.empty())
    {
        setStage(ExportStage::publicationOutbox);
        const auto publicationStart = juce::Time::getMillisecondCounterHiRes();
        SoftBindingPublication publication;
        SoftBindingPublicationRequest request;
        request.watermark = watermarkPayload;
        request.fingerprint = fingerprintRegistration;
        request.manifestBytes = manifestStore;
        request.title = destination.getFileName();
        request.claimGenerator = juce::String(appInfo::name.data());
        request.activeManifest = output.outputProvenance.activeManifest;
        output.result = publicationOutbox->publish(request, publication);
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

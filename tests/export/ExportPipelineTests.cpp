#include "engine/AudioEngine.h"
#include "watermark/SoftBindingStore.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>

namespace
{
struct ScopedTestDirectory
{
    ScopedTestDirectory()
        : root(juce::File::getCurrentWorkingDirectory()
                   .getNonexistentChildFile("c2paseq-export-pipeline", {}, false))
    {
        ready = root.createDirectory().wasOk();
    }
    ~ScopedTestDirectory() { root.deleteRecursively(); }
    juce::File root;
    bool ready = false;
};

bool writeTone(const juce::File& file, float amplitude, double seconds = 2.0)
{
    constexpr double sampleRate = 48000.0;
    const auto sampleCount = static_cast<int>(sampleRate * seconds);
    juce::AudioBuffer<float> source(1, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
        source.setSample(0, sample, amplitude * std::sin(
            juce::MathConstants<double>::twoPi * 220.0 * sample / sampleRate));
    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 1, 24, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(source, 0, sampleCount);
}

struct WavReadback
{
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::AudioBuffer<float> audio;
};

bool readWav(const juce::File& file, WavReadback& result)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    result.reader.reset(formats.createReaderFor(file));
    if (result.reader == nullptr || result.reader->lengthInSamples <= 0)
        return false;
    result.audio.setSize(static_cast<int>(result.reader->numChannels),
                         static_cast<int>(result.reader->lengthInSamples));
    return result.reader->read(&result.audio, 0, result.audio.getNumSamples(), 0, true, true);
}

double rmsAt(const WavReadback& wav, double seconds, double windowSeconds = 0.1)
{
    const auto start = static_cast<int>(seconds * wav.reader->sampleRate);
    const auto count = std::min(static_cast<int>(windowSeconds * wav.reader->sampleRate),
                                wav.audio.getNumSamples() - start);
    return count > 0 ? wav.audio.getRMSLevel(0, start, count) : 0.0;
}

bool approximately(double value, double expected, double tolerance)
{
    return std::abs(value - expected) <= tolerance;
}

int fail(int code, const juce::String& message)
{
    std::cerr << message << '\n';
    return code;
}

bool writeTestSigningBundle(const juce::File& destination)
{
    const auto certificates = juce::File(C2PA_TEST_CERTIFICATE_PEM).loadFileAsString();
    const auto privateKey = juce::File(C2PA_TEST_PRIVATE_KEY_PEM).loadFileAsString();
    return certificates.isNotEmpty() && privateKey.isNotEmpty()
        && destination.replaceWithText(certificates + "\n" + privateKey);
}

bool tamperAudioData(const juce::File& source, const juce::File& destination)
{
    if (! source.copyFileTo(destination))
        return false;
    juce::MemoryBlock bytes;
    if (! destination.loadFileAsData(bytes))
        return false;
    auto* data = static_cast<std::uint8_t*>(bytes.getData());
    std::size_t dataOffset = 12;
    while (dataOffset + 8 < bytes.getSize())
    {
        const auto chunkSize = static_cast<std::uint32_t>(data[dataOffset + 4])
            | (static_cast<std::uint32_t>(data[dataOffset + 5]) << 8)
            | (static_cast<std::uint32_t>(data[dataOffset + 6]) << 16)
            | (static_cast<std::uint32_t>(data[dataOffset + 7]) << 24);
        if (data[dataOffset] == 'd' && data[dataOffset + 1] == 'a'
            && data[dataOffset + 2] == 't' && data[dataOffset + 3] == 'a'
            && chunkSize > 256)
        {
            auto stream = destination.createOutputStream();
            if (stream == nullptr || ! stream->setPosition(static_cast<juce::int64>(dataOffset + 136)))
                return false;
            const auto changed = static_cast<std::uint8_t>(data[dataOffset + 136] ^ 0x01);
            return stream->writeByte(static_cast<char>(changed));
        }
        dataOffset += 8 + chunkSize + (chunkSize & 1u);
    }
    return false;
}

bool removeC2paChunk(const juce::File& source, const juce::File& destination)
{
    juce::MemoryBlock bytes;
    if (! source.loadFileAsData(bytes) || bytes.getSize() < 12)
        return false;
    const auto* data = static_cast<const std::uint8_t*>(bytes.getData());
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
        return false;
    juce::MemoryOutputStream body;
    body.write("WAVE", 4);
    std::size_t offset = 12;
    while (offset + 8 <= bytes.getSize())
    {
        const auto size = static_cast<std::uint32_t>(data[offset + 4])
            | (static_cast<std::uint32_t>(data[offset + 5]) << 8)
            | (static_cast<std::uint32_t>(data[offset + 6]) << 16)
            | (static_cast<std::uint32_t>(data[offset + 7]) << 24);
        const auto total = static_cast<std::size_t>(8 + size + (size & 1u));
        if (offset + total > bytes.getSize()) return false;
        const auto isC2pa = std::tolower(data[offset]) == 'c'
            && data[offset + 1] == '2' && std::tolower(data[offset + 2]) == 'p'
            && std::tolower(data[offset + 3]) == 'a';
        if (! isC2pa) body.write(data + offset, total);
        offset += total;
    }
    juce::MemoryOutputStream output;
    output.write("RIFF", 4);
    const auto bodySize = static_cast<std::uint32_t>(body.getDataSize());
    output.writeByte(static_cast<char>(bodySize & 0xff));
    output.writeByte(static_cast<char>((bodySize >> 8) & 0xff));
    output.writeByte(static_cast<char>((bodySize >> 16) & 0xff));
    output.writeByte(static_cast<char>((bodySize >> 24) & 0xff));
    output.write(body.getData(), body.getDataSize());
    return destination.replaceWithData(output.getData(), output.getDataSize());
}

class FakeWatermarkService final : public c2paseq::WatermarkService
{
public:
    bool isAvailable() const override { return available; }
    juce::String statusDescription() const override
    {
        return available ? "Ready (deterministic test double)" : "Setup required";
    }
    juce::Result embed(const juce::File& input, const juce::File& output,
                       const c2paseq::SoftBindingPayload& payload,
                       c2paseq::WatermarkEmbedResult& details) override
    {
        events.push_back("embed");
        embedded = payload;
        decodedOverride.reset();
        if (! input.copyFileTo(output)) return juce::Result::fail("fake embed copy failed");
        WavReadback wav;
        if (! readWav(output, wav)) return juce::Result::fail("fake embed readback failed");
        details.snrDb = 99.0;
        details.sampleRate = static_cast<int>(wav.reader->sampleRate);
        details.channels = static_cast<int>(wav.reader->numChannels);
        details.frames = wav.reader->lengthInSamples;
        return juce::Result::ok();
    }
    juce::Result decode(const juce::File&, c2paseq::SoftBindingPayload& payload) override
    {
        events.push_back("decode");
        if (failDecode) return juce::Result::fail("deterministic decode failure");
        if (! embedded.has_value() && ! decodedOverride.has_value())
            return juce::Result::fail("no deterministic watermark");
        payload = decodedOverride.value_or(*embedded);
        return juce::Result::ok();
    }

    bool available = true;
    bool failDecode = false;
    std::optional<c2paseq::SoftBindingPayload> embedded;
    std::optional<c2paseq::SoftBindingPayload> decodedOverride;
    std::vector<juce::String> events;
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    ScopedTestDirectory temporary;
    if (! temporary.ready)
        return fail(1, "could not create export test directory");

    const auto sourceA = temporary.root.getChildFile("A.wav");
    const auto sourceB = temporary.root.getChildFile("B.wav");
    const auto sourceC = temporary.root.getChildFile("C.wav");
    const auto unused = temporary.root.getChildFile("Unused.wav");
    const auto deleted = temporary.root.getChildFile("Deleted.wav");
    const auto testBundle = temporary.root.getChildFile("ephemeral-test-signing-bundle.pem");
    const auto signingConfiguration = temporary.root.getChildFile("signing-configuration");
    if (! writeTone(sourceA, 0.10f) || ! writeTone(sourceB, 0.20f, 1.0)
        || ! writeTone(sourceC, 0.05f, 1.0) || ! writeTone(unused, 0.30f, 1.0)
        || ! writeTone(deleted, 0.15f, 1.0))
        return fail(2, "could not create WAV fixtures");
    if (! writeTestSigningBundle(testBundle))
        return fail(2, "could not assemble upstream test signing fixture");

    const auto pluginCache = temporary.root.getChildFile("vst3-cache.xml");
    c2paseq::AudioEngine engine(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(
            signingConfiguration, false), pluginCache);
    juce::FileSearchPath pluginPaths;
    pluginPaths.add(juce::File(C2PASEQ_TEST_VST3_BUNDLE).getParentDirectory());
    if (engine.scanVst3Plugins(pluginPaths).failed())
        return fail(2, "could not scan deterministic VST3 fixture");
    const auto fixture = std::find_if(engine.availableVst3Plugins().begin(),
        engine.availableVst3Plugins().end(), [](const auto& plugin)
        { return plugin.name == "C2PA Test Gain"; });
    if (fixture == engine.availableVst3Plugins().end())
        return fail(2, "deterministic VST3 fixture was not discovered");
    const auto projectFolder = temporary.root.getChildFile("Export Test.c2paseq");
    if (const auto result = engine.createProject(projectFolder, "Export Test"); result.failed())
        return fail(3, result.getErrorMessage());
    const auto emptyFile = temporary.root.getChildFile("empty.wav");
    const auto empty = engine.exportMix(emptyFile);
    if (empty.result.wasOk() || emptyFile.existsAsFile())
        return fail(3, "empty project export did not fail cleanly");
    const auto sourceAHashBeforeImport = juce::SHA256(sourceA).toHexString();
    if (engine.importAudio(sourceA, 0, 1.0).failed()
        || engine.importAudio(sourceB, 0, 1.5).failed())
        return fail(4, "could not place same-track export fixtures");
    if (juce::SHA256(sourceA).toHexString() != sourceAHashBeforeImport)
        return fail(4, "automatic provenance inspection modified original source media");

    const auto missingCredentialFile = temporary.root.getChildFile("must-not-exist.wav");
    const auto missingCredential = engine.exportMix(missingCredentialFile);
    if (missingCredential.result.wasOk() || missingCredential.audioRendered
        || missingCredentialFile.existsAsFile()
        || ! missingCredential.result.getErrorMessage().contains("not been configured"))
        return fail(5, "missing credential silently produced an unsigned export");
    if (const auto configured = engine.configureSigningCredential(testBundle);
        configured.failed())
        return fail(5, configured.getErrorMessage());
    c2paseq::ProvenanceService relaunched(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(
            signingConfiguration, false));
    if (! relaunched.signingConfigured())
        return fail(5, "machine-local signing configuration did not survive restart");

    const auto occlusionFile = temporary.root.getChildFile("occlusion.wav");
    const auto occlusion = engine.exportMix(occlusionFile);
    if (occlusion.result.failed() || ! occlusion.audioRendered
        || ! occlusion.credentialsAttached || ! occlusion.credentialsValidated)
        return fail(6, "authenticated occlusion export failed: "
            + occlusion.result.getErrorMessage());
    if (occlusion.softBindingEnabled
        || occlusion.outputProvenance.rawManifestJson.contains("c2pa.soft-binding"))
        return fail(6, "disabled WavMark path changed the existing export manifest");
    WavReadback first;
    if (! readWav(occlusionFile, first) || first.reader->numChannels != 2
        || first.reader->bitsPerSample != 24)
        return fail(7, "signed export was not a reopenable stereo 24-bit WAV");
    if (! approximately(first.reader->lengthInSamples / first.reader->sampleRate, 3.0, 0.02))
        return fail(8, "export duration did not end at the last audible clip");
    const auto silence = rmsAt(first, 0.5);
    const auto aBefore = rmsAt(first, 1.25);
    const auto bPriority = rmsAt(first, 1.75);
    const auto aAfter = rmsAt(first, 2.75);
    if (silence > 0.0001 || aBefore < 0.02 || bPriority < aBefore * 1.75
        || bPriority > aBefore * 2.25 || ! approximately(aAfter / aBefore, 1.0, 0.08))
        return fail(9, "timing or same-track occlusion was incorrect in the rendered audio");

    if (engine.loadTrackPlugin(0, fixture->identifier).failed())
        return fail(9, "could not attach deterministic VST3 to export track");
    const auto processedFile = temporary.root.getChildFile("plugin-processed.wav");
    const auto processed = engine.exportMix(processedFile);
    WavReadback processedAudio;
    if (processed.result.failed() || ! processed.credentialsValidated
        || ! readWav(processedFile, processedAudio)
        || ! approximately(rmsAt(processedAudio, 1.25) / aBefore, 0.25, 0.04))
        return fail(9, "C2PA export did not contain the hosted VST3 processing result");
    const auto processedCredentials = engine.inspectProvenance(processedFile);
    if (! processedCredentials.c2paPresent || ! processedCredentials.assetIntact)
        return fail(9, "VST3-processed export lost valid Content Credentials");
    if (engine.saveProject().failed() || engine.openProject(projectFolder).failed())
        return fail(9, "hosted VST3 did not survive project reopen");
    const auto restoredTracks = engine.arrangementSnapshot();
    if (restoredTracks.empty() || ! restoredTracks[0].plugin.has_value()
        || restoredTracks[0].plugin->missing || restoredTracks[0].plugin->bypassed)
        return fail(9, "hosted VST3 identity/state did not restore");
    if (engine.removeTrackPlugin(0).failed())
        return fail(9, "could not remove deterministic VST3 after export regression test");

    if (engine.importAudio(sourceC, 1, 1.5).failed()
        || engine.importAudio(unused, 2, 1.5).failed()
        || engine.setTrackMute(2, true).failed())
        return fail(10, "could not place multitrack or muted fixtures");
    auto tracks = engine.arrangementSnapshot();
    const auto originalA = tracks[0].clips.front().id;
    if (engine.duplicateClip(originalA).failed())
        return fail(11, "could not duplicate repeated source fixture");
    if (engine.importAudio(deleted, 4, 0.0).failed())
        return fail(11, "could not register deleted-source fixture");
    tracks = engine.arrangementSnapshot();
    if (engine.deleteClip(tracks[4].clips.front().id).failed())
        return fail(11, "could not remove deleted-source fixture from arrangement");

    const auto mixedFile = temporary.root.getChildFile("mixed.wav");
    const auto mixed = engine.exportMix(mixedFile);
    if (mixed.result.failed() || mixed.ingredients.size() != 3)
        return fail(12, "ingredient deduplication or muted-source filtering failed");
    WavReadback second;
    if (! readWav(mixedFile, second) || rmsAt(second, 1.75) < bPriority * 1.18)
        return fail(13, "different tracks did not mix in the rendered audio");

    if (engine.setTrackSolo(1, true).failed())
        return fail(13, "could not set solo for export test");
    const auto soloFile = temporary.root.getChildFile("solo.wav");
    const auto solo = engine.exportMix(soloFile);
    if (solo.result.failed() || solo.ingredients.size() != 1
        || solo.ingredients.front().title != "C.wav")
        return fail(13, "soloed export did not isolate its audible source");
    if (engine.setTrackSolo(1, false).failed())
        return fail(13, "could not restore solo state after export test");

    if (engine.inspectProvenance(sourceA).status != c2paseq::ProvenanceStatus::noCredentials)
        return fail(14, "plain source ingest status was incorrect");
    const auto known = engine.inspectProvenance(juce::File(C2PA_KNOWN_SIGNED_WAV));
    if (! known.c2paPresent || known.activeManifest.isEmpty())
        return fail(15, "known authenticated source manifest was not read");
    if (engine.importAudio(juce::File(C2PA_KNOWN_SIGNED_WAV), 3, 0.0).failed()
        || engine.setTrackMute(3, true).failed())
        return fail(15, "known authenticated source could not be imported");
    tracks = engine.arrangementSnapshot();
    if (tracks[3].clips.empty() || ! tracks[3].clips.front().provenance.c2paPresent)
        return fail(15, "imported source provenance was not stored in project state");

    const auto signedFile = temporary.root.getChildFile("signed-export.wav");
    const auto signedResult = engine.exportMix(signedFile);
    if (signedResult.result.failed() || ! signedResult.credentialsAttached
        || ! signedResult.credentialsValidated)
        return fail(16, "signed export failed: " + signedResult.result.getErrorMessage());
    WavReadback signedAudio;
    if (! readWav(signedFile, signedAudio))
        return fail(17, "signed output was no longer a normal playable WAV");
    const auto signedInspection = engine.inspectProvenance(signedFile);
    if (! signedInspection.c2paPresent || ! signedInspection.assetIntact
        || ! signedInspection.rawManifestJson.contains("A.wav")
        || ! signedInspection.rawManifestJson.contains("B.wav")
        || ! signedInspection.rawManifestJson.contains("C.wav")
        || signedInspection.rawManifestJson.contains("Unused.wav")
        || signedInspection.rawManifestJson.contains("Deleted.wav")
        || signedInspection.rawManifestJson.contains("sample1_signed.wav"))
        return fail(18, "signed manifest did not represent exact contributing ingredients");

    if (engine.setTrackMute(3, false).failed())
        return fail(19, "could not enable authenticated ingredient");
    const auto propagatedFile = temporary.root.getChildFile("mixed-provenance.wav");
    const auto propagated = engine.exportMix(propagatedFile);
    const auto authenticatedIngredient = std::find_if(
        propagated.ingredients.begin(), propagated.ingredients.end(), [](const auto& ingredient)
        { return ingredient.provenance.c2paPresent; });
    if (propagated.result.failed() || authenticatedIngredient == propagated.ingredients.end()
        || ! propagated.outputProvenance.rawManifestJson.contains("sample1_signed.wav"))
        return fail(19, "mixed signed/unsigned ingredient provenance was not propagated");

    if (engine.importAudio(signedFile, 5, 0.0).failed())
        return fail(20, "exported WAV could not be reimported");
    tracks = engine.arrangementSnapshot();
    if (tracks.size() <= 5 || tracks[5].clips.empty()
        || ! tracks[5].clips.front().provenance.c2paPresent)
        return fail(20, "reimport did not automatically detect exported Content Credentials");

    const auto tampered = temporary.root.getChildFile("tampered.wav");
    if (! tamperAudioData(signedFile, tampered))
        return fail(21, "could not create tampered WAV fixture");
    const auto tamperedInspection = engine.inspectProvenance(tampered);
    if (tamperedInspection.assetIntact)
        return fail(22, "tampered protected audio still reported intact");

    const auto softConfiguration = temporary.root.getChildFile("soft-signing-configuration");
    const auto softStoreDirectory = temporary.root.getChildFile("soft-binding-store");
    auto fake = std::make_unique<FakeWatermarkService>();
    auto* fakePtr = fake.get();
    c2paseq::AudioEngine softEngine(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(
            softConfiguration, false),
        temporary.root.getChildFile("soft-vst3-cache.xml"), false,
        std::move(fake), softStoreDirectory);
    if (softEngine.configureSigningCredential(testBundle).failed())
        return fail(23, "could not configure soft-binding signing fixture");
    const auto softProject = temporary.root.getChildFile("Soft Binding.c2paseq");
    if (softEngine.createProject(softProject, "Soft Binding").failed()
        || softEngine.importAudio(sourceA, 0, 0.0).failed())
        return fail(24, "could not create deterministic soft-binding arrangement");
    softEngine.setSoftBindingEnabled(true);
    fakePtr->failDecode = true;
    const auto failedWatermark = temporary.root.getChildFile("must-not-fallback.wav");
    const auto failedWatermarkResult = softEngine.exportMix(failedWatermark);
    if (failedWatermarkResult.result.wasOk() || failedWatermark.existsAsFile()
        || failedWatermarkResult.stage != c2paseq::ExportStage::watermarkVerification)
        return fail(25, "failed watermark verification silently fell back to normal export");
    fakePtr->failDecode = false;
    fakePtr->events.clear();
    const auto softSigned = temporary.root.getChildFile("soft-signed.wav");
    const auto softResult = softEngine.exportMix(softSigned);
    if (softResult.result.failed() || ! softResult.watermarkVerified
        || softResult.softBindingPayloadHex.isEmpty()
        || fakePtr->events.size() < 2 || fakePtr->events[0] != "embed"
        || fakePtr->events[1] != "decode")
        return fail(26, "soft-binding export ordering/verification failed: "
            + softResult.result.getErrorMessage());
    const auto expectedPayload = c2paseq::SoftBindingPayload::fromHex(
        softResult.softBindingPayloadHex);
    if (! expectedPayload.has_value()
        || ! softResult.outputProvenance.rawManifestJson.contains("c2pa.watermarked.bound")
        || ! softResult.outputProvenance.rawManifestJson.contains("c2pa.soft-binding")
        || ! softResult.outputProvenance.rawManifestJson.contains(
            c2paseq::SoftBindingStore::algorithm)
        || ! c2paseq::ProvenanceService::hasMatchingSoftBinding(
            softResult.outputProvenance, *expectedPayload))
        return fail(27, "signed manifest did not contain exact WavMark C2PA assertions");
    juce::Array<juce::File> exactManifests;
    softStoreDirectory.getChildFile("manifests").findChildFiles(
        exactManifests, juce::File::findFiles, false, "*.c2pa");
    if (exactManifests.size() != 1
        || juce::SHA256(exactManifests[0]).toHexString()
            != softResult.softBindingManifestId)
        return fail(28, "Builder::sign manifest bytes were not persisted under their exact hash");
    WavReadback softWav;
    if (! readWav(softSigned, softWav) || softWav.reader->numChannels != 2
        || softWav.reader->sampleRate != first.reader->sampleRate
        || ! approximately(softWav.reader->lengthInSamples / softWav.reader->sampleRate,
                           2.0, 0.02))
        return fail(29, "soft-binding export did not preserve stereo/native-rate duration");

    const auto derivative = temporary.root.getChildFile("manifest-removed.wav");
    if (! removeC2paChunk(softSigned, derivative)
        || softEngine.inspectProvenance(derivative).status
            != c2paseq::ProvenanceStatus::noCredentials)
        return fail(30, "deterministic derivative retained embedded credentials");
    c2paseq::IngredientInfo recovered;
    if (softEngine.recoverProvenance(derivative, recovered).failed()
        || recovered.retrievalMode
            != c2paseq::ProvenanceRetrievalMode::recoveredSoftBinding
        || recovered.assetIntact || recovered.softBindingPayloadHex
            != softResult.softBindingPayloadHex)
        return fail(31, "manifestless derivative was not recovered with correct semantics");

    const auto decodesBeforeEmbedded = fakePtr->events.size();
    c2paseq::IngredientInfo embeddedPreferred;
    if (softEngine.recoverProvenance(softSigned, embeddedPreferred).failed()
        || embeddedPreferred.retrievalMode != c2paseq::ProvenanceRetrievalMode::embedded
        || fakePtr->events.size() != decodesBeforeEmbedded)
        return fail(32, "embedded-manifest validation was not preferred over soft recovery");

    fakePtr->decodedOverride = *c2paseq::SoftBindingPayload::fromHex("FFFF");
    if (softEngine.recoverProvenance(derivative, recovered).wasOk())
        return fail(33, "unknown WavMark payload produced false provenance");
    fakePtr->failDecode = true;
    if (softEngine.recoverProvenance(derivative, recovered).wasOk())
        return fail(34, "failed WavMark decode produced false provenance");
    fakePtr->failDecode = false;

    juce::Array<juce::File> manifests;
    softStoreDirectory.getChildFile("manifests").findChildFiles(
        manifests, juce::File::findFiles, false, "*.c2pa");
    juce::MemoryBlock storedBytes;
    const auto wrongPayload = *c2paseq::SoftBindingPayload::fromHex("B66B");
    c2paseq::SoftBindingStore testStore(softStoreDirectory);
    juce::String wrongId;
    if (manifests.size() != 1 || ! manifests[0].loadFileAsData(storedBytes))
        return fail(35, "could not read exact stored manifest fixture");
    const auto* storedData = static_cast<const std::uint8_t*>(storedBytes.getData());
    const std::vector<std::uint8_t> mismatchedManifest(
        storedData, storedData + storedBytes.getSize());
    if (testStore.persist(wrongPayload, mismatchedManifest,
                          "mismatched", wrongId).failed())
        return fail(36, "could not prepare mismatched recovery-manifest fixture");
    fakePtr->decodedOverride = wrongPayload;
    if (softEngine.recoverProvenance(derivative, recovered).wasOk())
        return fail(37, "lookup hit with mismatched manifest soft binding was accepted");

    if (juce::SystemStats::getEnvironmentVariable(
            "C2PASEQ_RUN_REAL_WAVMARK_TEST", {}) == "1")
    {
        const auto realStore = temporary.root.getChildFile("real-wavmark-store");
        c2paseq::AudioEngine realEngine(
            std::make_unique<c2paseq::ConformanceTestSigningProvider>(
                temporary.root.getChildFile("real-signing-configuration"), false),
            temporary.root.getChildFile("real-vst3-cache.xml"), false,
            std::make_unique<c2paseq::WavMarkService>(), realStore);
        if (realEngine.configureSigningCredential(testBundle).failed()
            || realEngine.createProject(
                temporary.root.getChildFile("Real WavMark.c2paseq"), "Real WavMark").failed()
            || realEngine.importAudio(sourceA, 0, 0.0).failed())
            return fail(38, "could not prepare opt-in real WavMark test");
        realEngine.setSoftBindingEnabled(true);
        const auto realSigned = temporary.root.getChildFile("real-wavmark-signed.wav");
        const auto realResult = realEngine.exportMix(realSigned);
        WavReadback realAudio;
        if (realResult.result.failed() || ! realResult.watermarkVerified
            || realResult.watermarkSnrDb < 20.0 || ! readWav(realSigned, realAudio)
            || realAudio.reader->numChannels != 2
            || realAudio.reader->sampleRate != first.reader->sampleRate)
            return fail(39, "real WavMark stereo/native-rate export failed: "
                + realResult.result.getErrorMessage());
        const auto realDerivative = temporary.root.getChildFile(
            "real-wavmark-manifest-removed.wav");
        c2paseq::IngredientInfo realRecovered;
        if (! removeC2paChunk(realSigned, realDerivative)
            || realEngine.inspectProvenance(realDerivative).c2paPresent
            || realEngine.recoverProvenance(realDerivative, realRecovered).failed()
            || realRecovered.retrievalMode
                != c2paseq::ProvenanceRetrievalMode::recoveredSoftBinding)
            return fail(40, "real WavMark manifestless recovery failed");
        std::cout << "real WavMark: stereo " << realAudio.reader->sampleRate
                  << " Hz, SNR " << realResult.watermarkSnrDb
                  << " dB, exact decode/sign/strip/recover passed\n";
    }

    const auto example = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_EXAMPLE_SIGNED_WAV", {});
    if (example.isNotEmpty() && ! signedFile.copyFileTo(juce::File(example)))
        return fail(41, "could not preserve requested example signed WAV");

    std::cout << "export pipeline: stereo 24-bit render, timing, duration, same-track "
                 "occlusion, multitrack mix, exact/mixed ingredients, mandatory signing, "
                 "reimport, persistence, tamper detection, optional soft-binding ordering, "
                 "exact manifest storage, recovery semantics, and negative cases passed\n";
    return 0;
}

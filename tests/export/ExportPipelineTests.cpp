#include "engine/AudioEngine.h"
#include "watermark/AudioWMarkService.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <future>
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

bool writeTone(const juce::File& file, float amplitude, double seconds = 2.0,
               int channels = 1)
{
    constexpr double sampleRate = 48000.0;
    const auto sampleCount = static_cast<int>(sampleRate * seconds);
    juce::AudioBuffer<float> source(channels, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
        for (int channel = 0; channel < channels; ++channel)
            source.setSample(channel, sample, amplitude * std::sin(
                juce::MathConstants<double>::twoPi * (220.0 + channel * 30.0)
                    * sample / sampleRate));
    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, channels, 24, {}, 0));
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

template <typename Future>
bool waitWhileDispatching(Future& future, int timeoutMilliseconds)
{
    const auto deadline = juce::Time::getMillisecondCounter() + timeoutMilliseconds;
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
    {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    return true;
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
                       c2paseq::WatermarkEmbedResult& details,
                       const std::function<bool()>& shouldCancel = {}) override
    {
        events.push_back("embed");
        embedded = payload;
        for (int wait = 0; wait < delayMilliseconds; wait += 10)
        {
            if (shouldCancel && shouldCancel())
                return juce::Result::fail("Export cancelled during deterministic embed");
            juce::Thread::sleep(10);
        }
        if (! input.copyFileTo(output)) return juce::Result::fail("fake embed copy failed");
        WavReadback wav;
        if (! readWav(output, wav)) return juce::Result::fail("fake embed readback failed");
        details.elapsedSeconds = delayMilliseconds / 1000.0;
        details.sampleRate = static_cast<int>(wav.reader->sampleRate);
        details.channels = static_cast<int>(wav.reader->numChannels);
        details.bitsPerSample = static_cast<int>(wav.reader->bitsPerSample);
        details.frames = wav.reader->lengthInSamples;
        return juce::Result::ok();
    }

    bool available = true;
    int delayMilliseconds = 0;
    std::optional<c2paseq::SoftBindingPayload> embedded;
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
        return fail(6, "disabled AudioWMark path changed the existing export manifest");
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
    const auto softOutboxDirectory = temporary.root.getChildFile("soft-binding-outbox");
    auto fake = std::make_unique<FakeWatermarkService>();
    auto* fakePtr = fake.get();
    c2paseq::AudioEngine softEngine(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(
            softConfiguration, false),
        temporary.root.getChildFile("soft-vst3-cache.xml"), false,
        std::move(fake), softOutboxDirectory);
    if (softEngine.configureSigningCredential(testBundle).failed())
        return fail(23, "could not configure soft-binding signing fixture");
    const auto softProject = temporary.root.getChildFile("Soft Binding.c2paseq");
    if (softEngine.createProject(softProject, "Soft Binding").failed()
        || softEngine.importAudio(sourceA, 0, 0.0).failed())
        return fail(24, "could not create deterministic soft-binding arrangement");
    softEngine.setSoftBindingEnabled(true);
    fakePtr->events.clear();
    fakePtr->delayMilliseconds = 150;
    const auto softSigned = temporary.root.getChildFile("soft-signed.wav");
    std::atomic_bool workerStarted { false };
    auto softFuture = std::async(std::launch::async, [&]
    {
        workerStarted.store(true);
        return softEngine.exportMix(softSigned);
    });
    while (! workerStarted.load()) juce::Thread::yield();
    if (softFuture.wait_for(std::chrono::milliseconds(25)) != std::future_status::timeout)
        return fail(25, "background export fixture did not exercise a responsive wait");
    if (! waitWhileDispatching(softFuture, 30000))
        return fail(25, "background export did not complete while the UI loop remained responsive");
    const auto softResult = softFuture.get();
    if (softResult.result.failed() || ! softResult.softBindingEnabled
        || ! softResult.audioPropertiesVerified
        || softResult.softBindingPayloadHex.length() != 32
        || fakePtr->events.size() != 1 || fakePtr->events[0] != "embed")
        return fail(26, "soft-binding export ordering failed: "
            + softResult.result.getErrorMessage());
    const auto expectedPayload = c2paseq::SoftBindingPayload::fromHex(
        softResult.softBindingPayloadHex);
    if (! expectedPayload.has_value()
        || ! softResult.outputProvenance.rawManifestJson.contains("c2pa.watermarked.bound")
        || ! softResult.outputProvenance.rawManifestJson.contains("c2pa.soft-binding")
        || ! softResult.outputProvenance.rawManifestJson.contains(
            c2paseq::audioWMarkAlgorithm.data())
        || ! c2paseq::ProvenanceService::hasMatchingSoftBinding(
            softResult.outputProvenance, *expectedPayload))
        return fail(27, "signed manifest did not contain exact AudioWMark C2PA assertions");
    const auto package = softOutboxDirectory.getChildFile(softResult.softBindingPayloadHex);
    const auto binding = juce::JSON::parse(
        package.getChildFile("binding.json").loadFileAsString());
    if (! package.isDirectory() || ! package.getChildFile("manifest.c2pa").existsAsFile()
        || ! binding.isObject()
        || binding["algorithm"].toString() != c2paseq::audioWMarkAlgorithm.data()
        || binding["value"].toString() != softResult.softBindingPayloadHex
        || binding["manifestId"].toString() != softResult.softBindingManifestId)
        return fail(28, "export did not publish a complete exact-manifest outbox package");
    WavReadback softWav;
    if (! readWav(softSigned, softWav) || softWav.reader->numChannels != 2
        || softWav.reader->sampleRate != first.reader->sampleRate
        || ! approximately(softWav.reader->lengthInSamples / softWav.reader->sampleRate,
                           2.0, 0.02))
        return fail(29, "soft-binding export did not preserve stereo/native-rate duration");

    const auto cancelledFile = temporary.root.getChildFile("cancelled-soft-binding.wav");
    std::atomic_bool cancel { false };
    auto cancelFuture = std::async(std::launch::async, [&]
    {
        return softEngine.exportMix(cancelledFile, {}, [&] { return cancel.load(); });
    });
    juce::Thread::sleep(25);
    cancel.store(true);
    if (! waitWhileDispatching(cancelFuture, 30000))
        return fail(30, "cancelled background export did not return promptly");
    const auto cancelled = cancelFuture.get();
    if (cancelled.result.wasOk() || cancelledFile.existsAsFile()
        || cancelled.stage != c2paseq::ExportStage::cancelled)
        return fail(30, "cancelled background export did not fail closed");

    if (juce::SystemStats::getEnvironmentVariable(
            "C2PASEQ_RUN_REAL_AUDIOWMARK_TEST", {}) == "1")
    {
        const auto realInput = temporary.root.getChildFile("real-audiowmark-input.wav");
        const auto realOutput = temporary.root.getChildFile("real-audiowmark-signed.wav");
        const auto derivative = temporary.root.getChildFile("real-audiowmark-derivative.wav");
        const auto realOutbox = temporary.root.getChildFile("real-audiowmark-outbox");
        const auto resolverRepository = temporary.root.getChildFile("real-resolver-repository");
        if (! writeTone(realInput, 0.1f, 10.0, 2))
            return fail(31, "could not prepare real AudioWMark fixture");
        c2paseq::AudioEngine realEngine(
            std::make_unique<c2paseq::ConformanceTestSigningProvider>(
                temporary.root.getChildFile("real-signing"), false),
            temporary.root.getChildFile("real-vst3-cache.xml"), false, {}, realOutbox);
        if (realEngine.configureSigningCredential(testBundle).failed()
            || realEngine.createProject(
                temporary.root.getChildFile("Real AudioWMark.c2paseq"), "Real AudioWMark").failed()
            || realEngine.importAudio(realInput, 0, 0.0).failed())
            return fail(31, "could not prepare real AudioWMark export project");
        realEngine.setSoftBindingEnabled(true);
        const auto realResult = realEngine.exportMix(realOutput);
        WavReadback realAudio;
        if (realResult.result.failed() || ! realResult.credentialsValidated
            || ! realResult.audioPropertiesVerified || ! readWav(realOutput, realAudio)
            || realAudio.reader->numChannels != 2 || realAudio.reader->bitsPerSample != 24
            || ! approximately(realAudio.reader->lengthInSamples
                    / realAudio.reader->sampleRate, 10.0, 0.02)
            || realResult.softBindingPayloadHex.length() != 32)
            return fail(32, "real AudioWMark signed export failed: "
                + realResult.result.getErrorMessage());
        if (! removeC2paChunk(realOutput, derivative)
            || realEngine.inspectProvenance(derivative).c2paPresent)
            return fail(32, "could not create the manifestless real-watermark derivative");
        juce::ChildProcess resolverCheck;
        const juce::StringArray command {
            "python3",
            juce::File(C2PASEQ_SOURCE_DIR).getChildFile(
                "tools/softbinding-resolver/integration_check.py").getFullPathName(),
            "--outbox", realOutbox.getFullPathName(),
            "--repository", resolverRepository.getFullPathName(),
            "--audio", derivative.getFullPathName(),
            "--value", realResult.softBindingPayloadHex,
            "--manifest-id", realResult.softBindingManifestId
        };
        if (! resolverCheck.start(command)
            || ! resolverCheck.waitForProcessToFinish(30000)
            || resolverCheck.getExitCode() != 0)
            return fail(32, "external resolver integration failed: "
                + resolverCheck.readAllProcessOutput());
        std::cout << "real AudioWMark: signed export, manifestless derivative, exact 128-bit "
                     "decode, resolver lookup, and exact manifest retrieval passed\n";
    }

    if (juce::SystemStats::getEnvironmentVariable(
            "C2PASEQ_RUN_AUDIOWMARK_BENCHMARK", {}) == "1")
    {
        std::cout << "| Audio duration | Render | AudioWMark embed | C2PA sign/validate | Total |\n"
                     "|---:|---:|---:|---:|---:|\n";
        for (const auto duration : { 10, 60, 180 })
        {
            const auto prefix = "benchmark-" + juce::String(duration);
            const auto input = temporary.root.getChildFile(prefix + "-input.wav");
            const auto output = temporary.root.getChildFile(prefix + "-output.wav");
            if (! writeTone(input, 0.1f, duration, 2))
                return fail(34, "could not create AudioWMark benchmark source");
            c2paseq::AudioEngine benchmarkEngine(
                std::make_unique<c2paseq::ConformanceTestSigningProvider>(
                    temporary.root.getChildFile(prefix + "-signing"), false),
                temporary.root.getChildFile(prefix + "-vst3-cache.xml"), false, {},
                temporary.root.getChildFile(prefix + "-outbox"));
            if (benchmarkEngine.configureSigningCredential(testBundle).failed()
                || benchmarkEngine.createProject(
                    temporary.root.getChildFile(prefix + ".c2paseq"), prefix).failed()
                || benchmarkEngine.importAudio(input, 0, 0.0).failed())
                return fail(35, "could not prepare AudioWMark benchmark project");
            benchmarkEngine.setSoftBindingEnabled(true);
            const auto result = benchmarkEngine.exportMix(output);
            if (result.result.failed() || ! result.credentialsValidated
                || ! result.audioPropertiesVerified)
                return fail(36, "AudioWMark benchmark export failed: "
                    + result.result.getErrorMessage());
            std::cout << "| " << duration << " s | " << result.renderSeconds << " s | "
                      << result.watermarkEmbedSeconds << " s | "
                      << result.signingAndValidationSeconds << " s | "
                      << result.totalSeconds << " s |\n";
        }
    }

    const auto example = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_EXAMPLE_SIGNED_WAV", {});
    if (example.isNotEmpty() && ! signedFile.copyFileTo(juce::File(example)))
        return fail(33, "could not preserve requested example signed WAV");

    std::cout << "export pipeline: stereo 24-bit render, timing, duration, same-track "
                 "occlusion, multitrack mix, exact/mixed ingredients, mandatory signing, "
                 "reimport, persistence, tamper detection, 128-bit soft-binding embed, "
                 "responsive worker execution, cancellation, and exact outbox publication passed\n";
    return 0;
}

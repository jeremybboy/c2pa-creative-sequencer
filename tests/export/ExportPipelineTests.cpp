#include "engine/AudioEngine.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>

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

    c2paseq::AudioEngine engine(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(
            signingConfiguration, false));
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

    const auto example = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_EXAMPLE_SIGNED_WAV", {});
    if (example.isNotEmpty() && ! signedFile.copyFileTo(juce::File(example)))
        return fail(23, "could not preserve requested example signed WAV");

    std::cout << "export pipeline: stereo 24-bit render, timing, duration, same-track "
                 "occlusion, multitrack mix, exact/mixed ingredients, mandatory signing, "
                 "reimport, persistence, reopen, and tamper detection passed\n";
    return 0;
}

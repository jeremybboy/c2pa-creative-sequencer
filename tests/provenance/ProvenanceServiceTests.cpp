#include "provenance/ProvenanceService.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <sys/stat.h>

namespace
{
bool writeTone(const juce::File& file, double frequency = 220.0)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 48000;
    juce::AudioBuffer<float> source(1, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
        source.setSample(0, sample, 0.2f * std::sin(
            juce::MathConstants<double>::twoPi * frequency * sample / sampleRate));
    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 1, 24, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(source, 0, sampleCount);
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

juce::var activeManifest(const c2paseq::IngredientInfo& info)
{
    const auto document = juce::JSON::parse(info.rawManifestJson);
    return document["manifests"][juce::Identifier(info.activeManifest)];
}

juce::var actionHistory(const c2paseq::IngredientInfo& info)
{
    const auto manifest = activeManifest(info);
    if (const auto* assertions = manifest["assertions"].getArray())
        for (const auto& assertion : *assertions)
            if (assertion["label"].toString().startsWith("c2pa.actions"))
                return assertion["data"]["actions"];
    return {};
}

bool equalJson(const juce::var& left, const juce::var& right)
{
    if (const auto* object = left.getDynamicObject())
    {
        const auto* other = right.getDynamicObject();
        if (other == nullptr || object->getProperties().size() != other->getProperties().size())
            return false;
        for (const auto& property : object->getProperties())
            if (! other->hasProperty(property.name)
                || ! equalJson(property.value, other->getProperty(property.name)))
                return false;
        return true;
    }
    if (const auto* array = left.getArray())
    {
        const auto* other = right.getArray();
        if (other == nullptr || array->size() != other->size()) return false;
        for (int index = 0; index < array->size(); ++index)
            if (! equalJson(array->getReference(index), other->getReference(index))) return false;
        return true;
    }
    return left == right;
}

bool readableWav(const juce::File& file)
{
    juce::WavAudioFormat format;
    const auto reader = std::unique_ptr<juce::AudioFormatReader>(
        format.createReaderFor(file.createInputStream().release(), true));
    return reader != nullptr && reader->sampleRate == 48000.0
        && reader->numChannels == 1 && reader->lengthInSamples == 48000;
}

bool matchesStem(const c2paseq::IngredientInfo& info,
                 const c2paseq::StemProvenanceDescriptor& descriptor)
{
    const auto manifest = activeManifest(info);
    const auto history = actionHistory(info);
    const auto* actions = history.getArray();
    if (! info.c2paPresent || ! info.assetIntact
        || manifest["title"].toString() != descriptor.title
        || actions == nullptr || actions->size() != static_cast<int>(descriptor.actions.size()))
        return false;
    for (int index = 0; index < actions->size(); ++index)
    {
        const auto& actual = actions->getReference(index);
        const auto& expected = descriptor.actions[static_cast<std::size_t>(index)];
        if (actual["action"].toString() != expected.action
            || actual["digitalSourceType"].toString() != expected.digitalSourceType
            || actual["description"].toString() != expected.description
            || actual["softwareAgent"]["name"].toString() != expected.softwareAgent)
            return false;
        if (expected.parameters.isVoid())
        {
            if (actual.hasProperty("parameters")) return false;
        }
        else if (! equalJson(actual["parameters"], expected.parameters))
            return false;
        if ((expected.description.isEmpty() && actual.hasProperty("description"))
            || (expected.softwareAgent.isEmpty() && actual.hasProperty("softwareAgent"))
            || (expected.digitalSourceType.isEmpty() && actual.hasProperty("digitalSourceType")))
            return false;
    }
    if (const auto* ingredients = manifest["ingredients"].getArray();
        ingredients != nullptr && ! ingredients->isEmpty())
        return false;
    if (const auto* assertions = manifest["assertions"].getArray())
        for (const auto& assertion : *assertions)
            if (assertion["label"].toString().startsWith("c2pa.soft-binding"))
                return false;
    return true;
}

bool noSuccessfulValidation(const c2paseq::IngredientInfo& info)
{
    return ! info.c2paPresent && ! info.assetIntact && info.activeManifest.isEmpty();
}
}

int main()
{
    const auto root = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-provenance", {}, false);
    if (root.createDirectory().failed())
        return fail(1, "could not create provenance test directory");

    const auto plain = root.getChildFile("plain.wav");
    const auto signedOutput = root.getChildFile("signed.wav");
    const auto configuration = root.getChildFile("configuration");
    const auto testBundle = root.getChildFile("ephemeral-test-signing-bundle.pem");
    if (! writeTone(plain))
        return fail(2, "could not create plain WAV fixture");
    if (! writeTestSigningBundle(testBundle))
        return fail(2, "could not assemble upstream test signing fixture");

    c2paseq::ProvenanceService service(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(configuration, false));
    if (service.signingConfigured())
        return fail(3, "isolated signer unexpectedly started configured");
    const auto invalid = root.getChildFile("invalid.pem");
    if (! invalid.replaceWithText("not a PEM")
        || service.configureSigningCredential(invalid).wasOk())
        return fail(3, "invalid credential was accepted");
    const auto noCredentials = service.inspect(plain);
    if (noCredentials.status != c2paseq::ProvenanceStatus::noCredentials
        || noCredentials.c2paPresent)
        return fail(4, "plain WAV was not classified NO_CREDENTIALS");

    const juce::File knownSigned(C2PA_KNOWN_SIGNED_WAV);
    const auto known = service.inspect(knownSigned);
    if (! known.c2paPresent || known.activeManifest.isEmpty())
        return fail(5, "known signed WAV manifest was not discovered");

    if (const auto configured = service.configureSigningCredential(testBundle);
        configured.failed())
        return fail(6, configured.getErrorMessage());
    const auto stored = configuration.getChildFile("signing-bundle.pem");
    struct stat fileStatus {};
    struct stat directoryStatus {};
    if (::stat(stored.getFullPathName().toRawUTF8(), &fileStatus) != 0
        || ::stat(configuration.getFullPathName().toRawUTF8(), &directoryStatus) != 0
        || (fileStatus.st_mode & 0777) != 0600 || (directoryStatus.st_mode & 0777) != 0700)
        return fail(7, "persisted credential permissions were not restrictive");

    c2paseq::ProvenanceService relaunched(
        std::make_unique<c2paseq::ConformanceTestSigningProvider>(configuration, false));
    if (! relaunched.signingConfigured())
        return fail(8, "credential configuration did not persist across service restart");

    c2paseq::IngredientInfo validation;
    const std::vector<c2paseq::ContributingIngredient> ingredients {
        { "plain", plain.getFileName(), juce::SHA256(plain).toHexString(), plain,
          noCredentials }
    };
    if (const auto result = relaunched.signWav(plain, signedOutput, ingredients,
                                               "Provenance Service Test", validation);
        result.failed())
        return fail(9, result.getErrorMessage());
    if (! validation.c2paPresent || ! validation.assetIntact
        || ! signedOutput.existsAsFile())
        return fail(10, "signed WAV did not validate after embedding");

    const auto mixHistory = actionHistory(validation);
    if (mixHistory.size() != 1 || mixHistory[0]["action"].toString() != "c2pa.created"
        || mixHistory[0]["digitalSourceType"].toString()
            != "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation"
        || mixHistory[0].hasProperty("description") || mixHistory[0].hasProperty("softwareAgent"))
        return fail(12, "existing unwatermarked final-mix action history changed");

    // These are synthetic schema/signing fixtures, NOT evidence of a MIDI render
    // or a human recording; workflow integration is tested separately.
    const auto sourceHash = juce::SHA256(plain).toHexString();
    const auto renderParameters = juce::JSON::parse(
        R"({"fixture":true,"track":{"name":"Schema fixture","noteCount":3},"labels":["one","two"]})");
    const c2paseq::StemProvenanceDescriptor midi {
        "Synthetic rendered-action fixture", "audio/wav",
        c2paseq::makeMidiRenderedStemActions(renderParameters) };
    const auto midiOutput = root.getChildFile("midi-actions.wav");
    if (const auto result = relaunched.signStemWav(plain, midiOutput, midi, validation);
        result.failed())
        return fail(13, result.getErrorMessage());
    if (! matchesStem(validation, midi) || ! matchesStem(relaunched.inspect(midiOutput), midi)
        || ! readableWav(midiOutput)
        || actionHistory(validation)[0]["action"].toString() != "c2pa.created"
        || actionHistory(validation)[1]["action"].toString() != "c2pa.rendered"
        || actionHistory(validation)[1]["softwareAgent"]["name"].toString()
            != "C2PA Creative Sequencer"
        || juce::SHA256(plain).toHexString() != sourceHash)
        return fail(14, "rendered stem fields did not round-trip or unsigned source changed");

    const auto captureSource = root.getChildFile("capture-fixture.wav");
    if (! writeTone(captureSource, 330.0)) return fail(15, "could not create second WAV fixture");
    const auto captureParameters = juce::JSON::parse(
        R"({"fixture":true,"input":{"name":"Synthetic test input"},"channels":1})");
    const c2paseq::StemProvenanceDescriptor capture {
        "Synthetic capture-action fixture", "audio/wav",
        c2paseq::makeHumanRecordedStemActions(captureParameters) };
    const auto captureOutput = root.getChildFile("capture-actions.wav");
    if (const auto result = relaunched.signStemWav(captureSource, captureOutput, capture, validation);
        result.failed())
        return fail(16, result.getErrorMessage());
    if (! matchesStem(validation, capture)
        || ! matchesStem(relaunched.inspect(captureOutput), capture)
        || ! readableWav(captureOutput)
        || actionHistory(validation)[0]["action"].toString() != "c2pa.created"
        || actionHistory(validation)[0]["digitalSourceType"].toString()
            != "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCapture")
        return fail(17, "digitalCapture stem fields did not round-trip");

    const c2paseq::StemProvenanceDescriptor minimal {
        "Optional-field fixture", "audio/wav", {
            { "c2pa.created", "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation",
              {}, {}, {} },
            { "c2pa.rendered", {}, {}, {}, {} } } };
    const auto minimalResult = relaunched.signStemWav(
        plain, root.getChildFile("minimal.wav"), minimal, validation);
    if (minimalResult.failed() || ! matchesStem(validation, minimal))
        return fail(18, "omitted optional fields did not round-trip");

    // Reuse a successful validation to prove failures cannot leave stale success.
    const auto successfulValidation = validation;
    auto incompleteHistory = minimal;
    incompleteHistory.actions[0].digitalSourceType.clear();
    if (relaunched.signStemWav(plain, root.getChildFile("invalid-history.wav"),
                               incompleteHistory, validation).wasOk()
        || validation.assetIntact)
        return fail(24, "invalid embedded action history bypassed post-sign validation");
    validation = successfulValidation;
    const auto failureOutput = root.getChildFile("failed.wav");
    if (relaunched.signStemWav(root.getChildFile("missing.wav"), failureOutput, midi, validation).wasOk()
        || ! noSuccessfulValidation(validation) || failureOutput.existsAsFile())
        return fail(19, "missing stem input did not fail cleanly");
    validation = successfulValidation;
    auto malformedDescriptor = midi;
    malformedDescriptor.actions[1].parameters = "not an object";
    if (relaunched.signStemWav(plain, failureOutput, malformedDescriptor, validation).wasOk()
        || ! noSuccessfulValidation(validation) || failureOutput.existsAsFile())
        return fail(20, "non-object action parameters were accepted");
    validation = successfulValidation;
    const auto malformedWav = root.getChildFile("malformed.wav");
    if (! malformedWav.replaceWithText("not WAV audio")
        || relaunched.signStemWav(malformedWav, failureOutput, midi, validation).wasOk()
        || ! noSuccessfulValidation(validation))
        return fail(21, "SDK signing failure retained a successful validation");
    validation = successfulValidation;
    if (! stored.replaceWithText("invalid configured credential")
        || relaunched.signStemWav(plain, root.getChildFile("invalid-signer.wav"), midi, validation).wasOk()
        || ! noSuccessfulValidation(validation))
        return fail(22, "invalid signing configuration retained a successful validation");
    if (relaunched.removeSigningCredential().failed() || relaunched.signingConfigured())
        return fail(11, "credential removal did not clear machine-local configuration");
    validation = successfulValidation;
    if (relaunched.signStemWav(plain, root.getChildFile("unconfigured.wav"), midi, validation).wasOk()
        || ! noSuccessfulValidation(validation))
        return fail(23, "unconfigured stem signer did not fail cleanly");

    root.deleteRecursively();
    std::cout << "provenance service: import states, credential validation/persistence, "
                 "signer construction, mix/stem actions, reopen validation, failure handling, "
                 "and removal passed\n";
    return 0;
}

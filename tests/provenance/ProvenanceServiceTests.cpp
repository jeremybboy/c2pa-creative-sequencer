#include "provenance/ProvenanceService.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <sys/stat.h>

namespace
{
bool writeTone(const juce::File& file)
{
    constexpr double sampleRate = 48000.0;
    constexpr int sampleCount = 48000;
    juce::AudioBuffer<float> source(1, sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
        source.setSample(0, sample, 0.2f * std::sin(
            juce::MathConstants<double>::twoPi * 220.0 * sample / sampleRate));
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
    if (relaunched.removeSigningCredential().failed() || relaunched.signingConfigured())
        return fail(11, "credential removal did not clear machine-local configuration");

    root.deleteRecursively();
    std::cout << "provenance service: import states, credential validation/persistence, "
                 "signer construction, signing, reopen validation, and removal passed\n";
    return 0;
}

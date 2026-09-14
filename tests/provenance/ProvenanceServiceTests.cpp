#include "provenance/ProvenanceService.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>

#include <cmath>
#include <iostream>
#include <memory>

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
}

int main()
{
    const auto root = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-provenance", {}, false);
    if (root.createDirectory().failed())
        return fail(1, "could not create provenance test directory");

    const auto plain = root.getChildFile("plain.wav");
    const auto signedOutput = root.getChildFile("signed.wav");
    if (! writeTone(plain))
        return fail(2, "could not create plain WAV fixture");

    c2paseq::ProvenanceService service;
    const auto noCredentials = service.inspect(plain);
    if (noCredentials.status != c2paseq::ProvenanceStatus::noCredentials
        || noCredentials.c2paPresent)
        return fail(3, "plain WAV was not classified NO_CREDENTIALS");

    const juce::File knownSigned(C2PA_KNOWN_SIGNED_WAV);
    const auto known = service.inspect(knownSigned);
    if (! known.c2paPresent || known.activeManifest.isEmpty())
        return fail(4, "known signed WAV manifest was not discovered");

    if (service.signingConfigured())
    {
        c2paseq::IngredientInfo validation;
        const std::vector<c2paseq::ContributingIngredient> ingredients {
            { "plain", plain.getFileName(), juce::SHA256(plain).toHexString(), plain,
              noCredentials }
        };
        if (const auto result = service.signWav(plain, signedOutput, ingredients,
                                                "Provenance Service Test", validation);
            result.failed())
            return fail(5, result.getErrorMessage());
        if (! validation.c2paPresent || ! validation.assetIntact
            || ! signedOutput.existsAsFile())
            return fail(6, "signed WAV did not validate after embedding");
    }

    root.deleteRecursively();
    std::cout << "provenance service: no-credentials and known-signed ingest passed";
    if (service.signingConfigured()) std::cout << "; configured signing passed";
    std::cout << '\n';
    return 0;
}

#include "SoftBindingOutbox.h"

namespace c2paseq
{
SoftBindingOutbox::SoftBindingOutbox(juce::File directory, PayloadGenerator generator)
    : root(std::move(directory)), payloadGenerator(std::move(generator))
{
}

juce::File SoftBindingOutbox::defaultDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("SoftBindingOutbox");
}

SoftBindingPayload SoftBindingOutbox::randomPayload() const
{
    if (payloadGenerator)
        return payloadGenerator();
    SoftBindingPayload payload;
    juce::Random::getSystemRandom().fillBitsRandomly(payload.bytes.data(), payload.bytes.size());
    return payload;
}

juce::Result SoftBindingOutbox::allocatePayload(SoftBindingPayload& payload) const
{
    for (int attempt = 0; attempt < 1024; ++attempt)
    {
        const auto candidate = randomPayload();
        if (! root.getChildFile(candidate.toHex()).exists())
        {
            payload = candidate;
            return juce::Result::ok();
        }
    }
    return juce::Result::fail("Could not allocate an unused 128-bit soft-binding identifier");
}

juce::Result SoftBindingOutbox::publish(const SoftBindingPayload& payload,
                                        const std::vector<std::uint8_t>& manifestBytes,
                                        const juce::String& title,
                                        const juce::String& claimGenerator,
                                        const juce::String& activeManifest,
                                        SoftBindingPublication& publication)
{
    SoftBindingPublicationRequest request;
    request.watermark = payload;
    request.manifestBytes = manifestBytes;
    request.title = title;
    request.claimGenerator = claimGenerator;
    request.activeManifest = activeManifest;
    return publish(request, publication);
}

juce::Result SoftBindingOutbox::publish(const SoftBindingPublicationRequest& request,
                                        SoftBindingPublication& publication)
{
    if (request.manifestBytes.empty() || request.activeManifest.isEmpty())
        return juce::Result::fail("C2PA returned no publishable signed manifest store");
    if (! request.watermark.has_value() && ! request.fingerprint.has_value())
        return juce::Result::fail("Soft-binding publication has no registration material");
    if (auto result = root.createDirectory(); result.failed())
        return juce::Result::fail("Could not create the soft-binding publication outbox");

    const auto manifestSuffix = request.activeManifest
        .fromLastOccurrenceOf(":", false, false)
        .retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-");
    const auto publicationId = request.watermark.has_value()
        ? request.watermark->toHex()
        : request.fingerprint->valueHex.substring(0, 32) + "-" + manifestSuffix;
    const auto destination = root.getChildFile(publicationId);
    if (destination.exists())
        return juce::Result::fail("Soft-binding publication identifier already exists");
    const auto staging = root.getNonexistentChildFile(publicationId + ".partial", {}, false);
    if (auto result = staging.createDirectory(); result.failed())
        return juce::Result::fail("Could not create a temporary publication package");

    const auto manifest = staging.getChildFile("manifest.c2pa");
    auto metadata = std::make_unique<juce::DynamicObject>();
    metadata->setProperty("schemaVersion", 2);
    metadata->setProperty("publicationId", publicationId);
    metadata->setProperty("title", request.title);
    metadata->setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true));
    metadata->setProperty("claimGenerator", request.claimGenerator);
    metadata->setProperty("manifestId", request.activeManifest);
    metadata->setProperty("manifestFile", "manifest.c2pa");
    juce::Array<juce::var> bindings;
    if (request.watermark.has_value())
    {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("type", "watermark");
        item->setProperty("algorithm", juce::String(audioWMarkAlgorithm.data()));
        item->setProperty("value", request.watermark->toHex());
        bindings.add(item.release());
        // Preserve PR 011's top-level fields for old resolver imports.
        metadata->setProperty("algorithm", juce::String(audioWMarkAlgorithm.data()));
        metadata->setProperty("value", request.watermark->toHex());
    }
    if (request.fingerprint.has_value())
    {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("type", "fingerprint");
        item->setProperty("algorithm", juce::String(audfprintAlgorithm.data()));
        item->setProperty("value", request.fingerprint->valueHex);
        item->setProperty("registrationFile", "fingerprint-data.afpt");
        bindings.add(item.release());
    }
    metadata->setProperty("bindings", bindings);
    const auto binding = staging.getChildFile("binding.json");
    if (! manifest.replaceWithData(request.manifestBytes.data(), request.manifestBytes.size())
        || ! binding.replaceWithText(juce::JSON::toString(juce::var(metadata.release()), true)))
    {
        staging.deleteRecursively();
        return juce::Result::fail("Could not write the publication package");
    }
    if (request.fingerprint.has_value())
    {
        const auto& fingerprint = *request.fingerprint;
        auto details = std::make_unique<juce::DynamicObject>();
        details->setProperty("schemaVersion", 1);
        details->setProperty("type", "fingerprint");
        details->setProperty("algorithm", juce::String(audfprintAlgorithm.data()));
        details->setProperty("value", fingerprint.valueHex);
        details->setProperty("engine", "dpwe/audfprint");
        details->setProperty("engineCommit", fingerprint.engineVersion);
        details->setProperty("profile", "sr=11025;fft=512;hop=256;density=20;shifts=1");
        details->setProperty("hashCount", fingerprint.hashCount);
        details->setProperty("durationSeconds", fingerprint.durationSeconds);
        details->setProperty("registrationFile", "fingerprint-data.afpt");
        if (! fingerprint.artifact.existsAsFile()
            || ! fingerprint.artifact.copyFileTo(staging.getChildFile("fingerprint-data.afpt"))
            || ! staging.getChildFile("fingerprint.json").replaceWithText(
                juce::JSON::toString(juce::var(details.release()), true)))
        {
            staging.deleteRecursively();
            return juce::Result::fail("Could not publish audfprint registration material");
        }
    }
    if (! staging.moveFileTo(destination))
    {
        staging.deleteRecursively();
        return juce::Result::fail("Could not atomically publish the soft-binding package");
    }
    publication = { request.watermark.value_or(SoftBindingPayload {}),
                    request.activeManifest, destination };
    return juce::Result::ok();
}
}

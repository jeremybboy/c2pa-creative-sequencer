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
    if (manifestBytes.empty() || activeManifest.isEmpty())
        return juce::Result::fail("C2PA returned no publishable signed manifest store");
    if (auto result = root.createDirectory(); result.failed())
        return juce::Result::fail("Could not create the soft-binding publication outbox");

    const auto destination = root.getChildFile(payload.toHex());
    if (destination.exists())
        return juce::Result::fail("Soft-binding publication identifier already exists");
    const auto staging = root.getNonexistentChildFile(payload.toHex() + ".partial", {}, false);
    if (auto result = staging.createDirectory(); result.failed())
        return juce::Result::fail("Could not create a temporary publication package");

    const auto manifest = staging.getChildFile("manifest.c2pa");
    auto metadata = std::make_unique<juce::DynamicObject>();
    metadata->setProperty("algorithm", juce::String(audioWMarkAlgorithm.data()));
    metadata->setProperty("value", payload.toHex());
    metadata->setProperty("title", title);
    metadata->setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true));
    metadata->setProperty("claimGenerator", claimGenerator);
    metadata->setProperty("manifestId", activeManifest);
    metadata->setProperty("manifestFile", "manifest.c2pa");
    const auto binding = staging.getChildFile("binding.json");
    if (! manifest.replaceWithData(manifestBytes.data(), manifestBytes.size())
        || ! binding.replaceWithText(juce::JSON::toString(juce::var(metadata.release()), true)))
    {
        staging.deleteRecursively();
        return juce::Result::fail("Could not write the publication package");
    }
    if (! staging.moveFileTo(destination))
    {
        staging.deleteRecursively();
        return juce::Result::fail("Could not atomically publish the soft-binding package");
    }
    publication = { payload, activeManifest, destination };
    return juce::Result::ok();
}
}

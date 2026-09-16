#include "SoftBindingStore.h"

#include <juce_cryptography/juce_cryptography.h>

namespace c2paseq
{
SoftBindingStore::SoftBindingStore(juce::File directory, PayloadGenerator generator)
    : root(std::move(directory)), payloadGenerator(std::move(generator))
{
}

juce::File SoftBindingStore::defaultDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support")
        .getChildFile("C2PA Creative Sequencer")
        .getChildFile("SoftBindingStore");
}

juce::String SoftBindingStore::keyFor(const SoftBindingPayload& payload) const
{
    return juce::String(algorithm) + ":" + payload.toHex();
}

juce::Result SoftBindingStore::readIndex(juce::var& index) const
{
    const auto file = root.getChildFile("index.json");
    if (! file.existsAsFile())
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("version", 1);
        object->setProperty("bindings", new juce::DynamicObject());
        index = object.release();
        return juce::Result::ok();
    }
    const auto result = juce::JSON::parse(file.loadFileAsString(), index);
    auto* object = index.getDynamicObject();
    if (result.failed() || object == nullptr
        || object->getProperty("bindings").getDynamicObject() == nullptr)
        return juce::Result::fail("Soft-binding recovery index is invalid");
    return juce::Result::ok();
}

juce::Result SoftBindingStore::writeIndex(const juce::var& index) const
{
    if (auto result = root.createDirectory(); result.failed())
        return juce::Result::fail("Could not create local soft-binding store");
    const auto destination = root.getChildFile("index.json");
    juce::TemporaryFile temporary(destination);
    if (! temporary.getFile().replaceWithText(juce::JSON::toString(index, true)))
        return juce::Result::fail("Could not write local soft-binding index");
    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not atomically commit local soft-binding index");
    return juce::Result::ok();
}

SoftBindingPayload SoftBindingStore::randomPayload() const
{
    if (payloadGenerator) return payloadGenerator();
    const auto value = juce::Random::getSystemRandom().nextInt(0x10000);
    return { { static_cast<std::uint8_t>((value >> 8) & 0xff),
                 static_cast<std::uint8_t>(value & 0xff) } };
}

bool SoftBindingStore::contains(const SoftBindingPayload& payload) const
{
    juce::var index;
    if (readIndex(index).failed()) return false;
    auto* bindings = index.getProperty("bindings", {}).getDynamicObject();
    return bindings != nullptr && bindings->hasProperty(keyFor(payload));
}

juce::Result SoftBindingStore::allocatePayload(SoftBindingPayload& payload) const
{
    juce::var index;
    if (const auto result = readIndex(index); result.failed()) return result;
    auto* bindings = index.getProperty("bindings", {}).getDynamicObject();
    for (int attempt = 0; attempt < 65536; ++attempt)
    {
        const auto candidate = randomPayload();
        if (bindings == nullptr || ! bindings->hasProperty(keyFor(candidate)))
        {
            payload = candidate;
            return juce::Result::ok();
        }
    }
    return juce::Result::fail("No unused 16-bit WavMark payload remains in the local store");
}

juce::Result SoftBindingStore::persist(const SoftBindingPayload& payload,
                                       const std::vector<std::uint8_t>& manifestBytes,
                                       const juce::String& title,
                                       juce::String& manifestId)
{
    if (manifestBytes.empty())
        return juce::Result::fail("C2PA returned an empty manifest store");
    juce::var index;
    if (const auto result = readIndex(index); result.failed()) return result;
    auto* bindings = index.getProperty("bindings", {}).getDynamicObject();
    if (bindings == nullptr)
        return juce::Result::fail("Soft-binding recovery index has no bindings map");
    if (bindings->hasProperty(keyFor(payload)))
        return juce::Result::fail("Soft-binding payload collision detected before persistence");

    manifestId = juce::SHA256(manifestBytes.data(), manifestBytes.size()).toHexString();
    const auto manifests = root.getChildFile("manifests");
    if (auto result = manifests.createDirectory(); result.failed())
        return juce::Result::fail("Could not create local manifest directory");
    const auto manifestFile = manifests.getChildFile(manifestId + ".c2pa");
    juce::TemporaryFile manifestTemporary(manifestFile);
    if (! manifestTemporary.getFile().replaceWithData(manifestBytes.data(), manifestBytes.size())
        || ! manifestTemporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not atomically persist exact C2PA manifest bytes");

    auto entry = std::make_unique<juce::DynamicObject>();
    entry->setProperty("algorithm", algorithm);
    entry->setProperty("payload_hex", payload.toHex());
    entry->setProperty("manifest_id", manifestId);
    entry->setProperty("path", "manifests/" + manifestId + ".c2pa");
    entry->setProperty("title", title);
    entry->setProperty("created_utc", juce::Time::getCurrentTime().toISO8601(true));
    bindings->setProperty(keyFor(payload), entry.release());
    return writeIndex(index);
}

juce::Result SoftBindingStore::resolve(const SoftBindingPayload& payload,
                                       StoredSoftBinding& stored) const
{
    juce::var index;
    if (const auto result = readIndex(index); result.failed()) return result;
    auto* bindings = index.getProperty("bindings", {}).getDynamicObject();
    auto* entry = bindings != nullptr
        ? bindings->getProperty(keyFor(payload)).getDynamicObject() : nullptr;
    if (entry == nullptr)
        return juce::Result::fail("No local recovery manifest matches WavMark payload "
                                  + payload.toHex());
    if (entry->getProperty("algorithm").toString() != algorithm
        || entry->getProperty("payload_hex").toString() != payload.toHex())
        return juce::Result::fail("Local recovery index entry does not match algorithm and payload");
    const auto relativePath = entry->getProperty("path").toString();
    const auto file = root.getChildFile(relativePath);
    juce::MemoryBlock data;
    if (! file.loadFileAsData(data) || data.getSize() == 0)
        return juce::Result::fail("Stored C2PA manifest bytes are missing");
    stored.payload = payload;
    stored.algorithm = algorithm;
    stored.manifestId = entry->getProperty("manifest_id").toString();
    stored.title = entry->getProperty("title").toString();
    stored.createdUtc = entry->getProperty("created_utc").toString();
    const auto* bytes = static_cast<const std::uint8_t*>(data.getData());
    stored.manifestBytes.assign(bytes, bytes + data.getSize());
    return juce::Result::ok();
}
}

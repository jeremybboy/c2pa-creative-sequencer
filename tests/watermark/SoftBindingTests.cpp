#include "watermark/SoftBindingOutbox.h"
#include "watermark/SoftBindingPayload.h"

#include <cstring>
#include <iostream>

namespace
{
int fail(int code, const juce::String& message)
{
    std::cerr << message << '\n';
    return code;
}
}

int main()
{
    const auto root = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-soft-binding-outbox", {}, false);
    if (root.createDirectory().failed()) return fail(1, "could not create test outbox");

    constexpr auto hex = "0123456789abcdef0011223344556677";
    const auto payload = c2paseq::SoftBindingPayload::fromHex(hex);
    if (! payload.has_value() || payload->toHex() != hex
        || payload->toBitString().length() != 128
        || c2paseq::SoftBindingPayload::fromBitString(payload->toBitString()) != payload)
        return fail(2, "canonical 128-bit payload conversion failed");
    if (c2paseq::SoftBindingPayload::fromHex("a55a").has_value()
        || c2paseq::SoftBindingPayload::fromHex("xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxy").has_value()
        || c2paseq::SoftBindingPayload::fromBitString("001").has_value())
        return fail(3, "invalid payload representation was accepted");

    int generated = 0;
    const auto alternate = *c2paseq::SoftBindingPayload::fromHex(
        "fedcba9876543210ffeeddccbbaa9988");
    c2paseq::SoftBindingOutbox outbox(root, [&]
    {
        ++generated;
        return generated == 1 ? *payload : alternate;
    });
    root.getChildFile(payload->toHex()).createDirectory();
    c2paseq::SoftBindingPayload allocated;
    if (outbox.allocatePayload(allocated).failed() || allocated != alternate || generated != 2)
        return fail(4, "payload collision was not retried");

    const std::vector<std::uint8_t> exactBytes { 0, 1, 2, 3, 0xfe, 0xff };
    c2paseq::SoftBindingPublication publication;
    if (outbox.publish(alternate, exactBytes, "test.wav", "test-generator",
                       "urn:uuid:test-manifest", publication).failed())
        return fail(5, "could not publish exact manifest bytes");
    juce::MemoryBlock persisted;
    const auto manifest = publication.packageDirectory.getChildFile("manifest.c2pa");
    const auto binding = publication.packageDirectory.getChildFile("binding.json");
    if (! manifest.loadFileAsData(persisted) || persisted.getSize() != exactBytes.size()
        || std::memcmp(persisted.getData(), exactBytes.data(), exactBytes.size()) != 0)
        return fail(6, "outbox did not preserve exact manifest bytes");
    const auto metadata = juce::JSON::parse(binding.loadFileAsString());
    if (! metadata.isObject()
        || metadata["algorithm"].toString() != c2paseq::audioWMarkAlgorithm.data()
        || metadata["value"].toString() != alternate.toHex()
        || metadata["manifestId"].toString() != "urn:uuid:test-manifest")
        return fail(7, "outbox binding metadata was incorrect");
    if (outbox.publish(alternate, exactBytes, "duplicate", "test-generator",
                       "urn:uuid:test-manifest", publication).wasOk())
        return fail(8, "payload collision was accepted during publication");

    root.deleteRecursively();
    std::cout << "soft binding: 128-bit payload, collision retry, exact-byte outbox, "
                 "and binding metadata passed\n";
    return 0;
}

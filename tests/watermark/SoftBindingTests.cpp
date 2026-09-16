#include "watermark/SoftBindingPayload.h"
#include "watermark/SoftBindingStore.h"

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
        .getNonexistentChildFile("c2paseq-soft-binding", {}, false);
    if (root.createDirectory().failed()) return fail(1, "could not create test store");

    const auto payload = c2paseq::SoftBindingPayload::fromHex("A55A");
    if (! payload.has_value() || payload->toBitString() != "1010010101011010"
        || payload->toBase64() != "pVo="
        || c2paseq::SoftBindingPayload::fromBitString(payload->toBitString()) != payload)
        return fail(2, "canonical payload conversion failed");
    if (c2paseq::SoftBindingPayload::fromHex("XYZ1").has_value()
        || c2paseq::SoftBindingPayload::fromHex("12-4").has_value()
        || c2paseq::SoftBindingPayload::fromBitString("001").has_value()
        || c2paseq::SoftBindingPayload::fromBitString("aaaaaaaaaaaaaaaa").has_value())
        return fail(3, "invalid payload representation was accepted");

    int generated = 0;
    c2paseq::SoftBindingStore store(root, [&]
    {
        ++generated;
        return generated == 1 ? *payload
            : *c2paseq::SoftBindingPayload::fromHex("B66B");
    });
    const std::vector<std::uint8_t> exactBytes { 0, 1, 2, 3, 0xfe, 0xff };
    juce::String manifestId;
    if (store.persist(*payload, exactBytes, "first", manifestId).failed())
        return fail(4, "could not persist exact manifest bytes");
    c2paseq::StoredSoftBinding recovered;
    if (store.resolve(*payload, recovered).failed() || recovered.manifestBytes != exactBytes
        || recovered.algorithm != c2paseq::SoftBindingStore::algorithm)
        return fail(5, "store did not return exact manifest bytes");
    c2paseq::SoftBindingPayload allocated;
    if (store.allocatePayload(allocated).failed() || allocated.toHex() != "B66B"
        || generated != 2)
        return fail(6, "payload collision was not retried");
    if (store.resolve(allocated, recovered).wasOk())
        return fail(7, "unknown payload produced false recovery");
    if (store.persist(*payload, exactBytes, "duplicate", manifestId).wasOk())
        return fail(8, "payload collision was accepted during persistence");

    root.deleteRecursively();
    std::cout << "soft binding: canonical payload, exact-byte store, collision retry, "
                 "and unknown lookup passed\n";
    return 0;
}

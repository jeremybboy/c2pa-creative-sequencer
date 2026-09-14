#include "ProvenanceService.h"

#include "app/AppInfo.h"

#include <c2pa.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace c2paseq
{
namespace
{
std::shared_ptr<c2pa::Context> makeContext()
{
    return std::make_shared<c2pa::Context>();
}

void collectValidationIssues(const juce::var& value, std::vector<juce::String>& issues)
{
    if (auto* array = value.getArray())
    {
        for (const auto& child : *array)
            collectValidationIssues(child, issues);
        return;
    }

    auto* object = value.getDynamicObject();
    if (object == nullptr)
        return;

    if (object->hasProperty("validation_status"))
    {
        if (auto* statuses = object->getProperty("validation_status").getArray())
            for (const auto& status : *statuses)
                if (auto* statusObject = status.getDynamicObject())
                {
                    const auto code = statusObject->getProperty("code").toString();
                    if (code.isNotEmpty()
                        && std::find(issues.begin(), issues.end(), code) == issues.end())
                        issues.push_back(code);
                }
    }

    for (const auto& property : object->getProperties())
        if (property.name.toString() != "validation_status")
            collectValidationIssues(property.value, issues);
}

bool isIntegrityFailure(const juce::String& issue)
{
    const auto lower = issue.toLowerCase();
    return lower.contains("mismatch") || lower.contains("invalid")
        || lower.contains("malformed") || lower.contains("hardbinding");
}

juce::String firstString(juce::DynamicObject* object,
                         std::initializer_list<const char*> names)
{
    if (object == nullptr)
        return {};
    for (const auto* name : names)
    {
        const auto value = object->getProperty(name).toString();
        if (value.isNotEmpty())
            return value;
    }
    return {};
}

IngredientInfo parseManifest(const std::string& manifestJson)
{
    IngredientInfo info;
    info.c2paPresent = true;
    info.rawManifestJson = juce::String::fromUTF8(manifestJson.c_str());

    juce::var document;
    const auto parseResult = juce::JSON::parse(info.rawManifestJson, document);
    auto* root = document.getDynamicObject();
    if (parseResult.failed() || root == nullptr)
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.validationSummary = "Manifest JSON could not be parsed";
        return info;
    }

    info.activeManifest = root->getProperty("active_manifest").toString();
    auto* manifests = root->getProperty("manifests").getDynamicObject();
    auto* active = manifests != nullptr
        ? manifests->getProperty(info.activeManifest).getDynamicObject() : nullptr;
    if (active != nullptr)
    {
        if (auto* generators = active->getProperty("claim_generator_info").getArray();
            generators != nullptr && ! generators->isEmpty())
        {
            auto* generator = generators->getFirst().getDynamicObject();
            info.claimGenerator = firstString(generator, { "name" });
            const auto version = firstString(generator, { "version" });
            if (version.isNotEmpty()) info.claimGenerator += " " + version;
        }
        if (info.claimGenerator.isEmpty())
            info.claimGenerator = active->getProperty("claim_generator").toString();

        auto* signature = active->getProperty("signature_info").getDynamicObject();
        info.signer = firstString(signature,
            { "common_name", "issuer", "cert_serial_number" });
    }

    collectValidationIssues(document, info.validationIssues);
    info.assetIntact = std::none_of(info.validationIssues.begin(), info.validationIssues.end(),
                                    isIntegrityFailure);
    if (info.activeManifest.isEmpty())
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.assetIntact = false;
        info.validationSummary = "No active manifest was reported";
    }
    else if (info.validationIssues.empty())
    {
        info.status = ProvenanceStatus::valid;
        info.validationSummary = "Manifest and protected asset validated";
    }
    else
    {
        info.status = ProvenanceStatus::presentWithValidationIssue;
        info.validationSummary = juce::String(info.validationIssues.size())
            + (info.validationIssues.size() == 1 ? " validation issue" : " validation issues");
    }
    return info;
}

juce::String makeManifestDefinition(const juce::String& title)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("claim_version", 2);
    root->setProperty("title", title);
    root->setProperty("format", "audio/wav");

    auto generator = std::make_unique<juce::DynamicObject>();
    generator->setProperty("name", juce::String(appInfo::name.data()));
    generator->setProperty("version", juce::String(appInfo::version.data()));
    juce::Array<juce::var> generators;
    generators.add(generator.release());
    root->setProperty("claim_generator_info", generators);

    auto action = std::make_unique<juce::DynamicObject>();
    action->setProperty("action", "c2pa.created");
    action->setProperty("digitalSourceType",
        "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation");
    juce::Array<juce::var> actions;
    actions.add(action.release());
    auto actionData = std::make_unique<juce::DynamicObject>();
    actionData->setProperty("actions", actions);
    auto assertion = std::make_unique<juce::DynamicObject>();
    assertion->setProperty("label", "c2pa.actions");
    assertion->setProperty("data", actionData.release());
    juce::Array<juce::var> assertions;
    assertions.add(assertion.release());
    root->setProperty("assertions", assertions);
    return juce::JSON::toString(juce::var(root.release()), false);
}

juce::String makeIngredientDefinition(const ContributingIngredient& ingredient)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("title", ingredient.title);
    object->setProperty("relationship", "componentOf");
    return juce::JSON::toString(juce::var(object.release()), false);
}

std::string extractPemBlock(const juce::String& bundle,
                            const juce::String& beginMarker,
                            const juce::String& endMarker)
{
    const auto start = bundle.indexOf(beginMarker);
    if (start < 0)
        return {};
    const auto end = bundle.indexOf(start, endMarker);
    if (end < 0)
        return {};
    return bundle.substring(start, end + endMarker.length()).toStdString() + "\n";
}

void appendDerLength(std::vector<std::uint8_t>& output, std::size_t length)
{
    if (length < 128)
    {
        output.push_back(static_cast<std::uint8_t>(length));
        return;
    }
    std::vector<std::uint8_t> bytes;
    while (length > 0)
    {
        bytes.push_back(static_cast<std::uint8_t>(length & 0xff));
        length >>= 8;
    }
    output.push_back(static_cast<std::uint8_t>(0x80 | bytes.size()));
    output.insert(output.end(), bytes.rbegin(), bytes.rend());
}

void appendDer(std::vector<std::uint8_t>& output, std::uint8_t tag,
               const std::vector<std::uint8_t>& value)
{
    output.push_back(tag);
    appendDerLength(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

std::string sec1EcKeyToPkcs8(const juce::String& sec1Pem)
{
    const auto body = sec1Pem.fromFirstOccurrenceOf(
        "-----BEGIN EC PRIVATE KEY-----", false, false)
        .upToFirstOccurrenceOf("-----END EC PRIVATE KEY-----", false, false)
        .removeCharacters("\r\n \t");
    juce::MemoryOutputStream decoded;
    if (! juce::Base64::convertFromBase64(decoded, body))
        return {};
    const auto* bytes = static_cast<const std::uint8_t*>(decoded.getData());
    std::vector<std::uint8_t> sec1(bytes, bytes + decoded.getDataSize());

    const std::vector<std::uint8_t> algorithmIdentifier {
        0x30, 0x13,
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
        0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
    };
    std::vector<std::uint8_t> privateKeyInfo { 0x02, 0x01, 0x00 };
    privateKeyInfo.insert(privateKeyInfo.end(), algorithmIdentifier.begin(),
                          algorithmIdentifier.end());
    appendDer(privateKeyInfo, 0x04, sec1);
    std::vector<std::uint8_t> pkcs8;
    appendDer(pkcs8, 0x30, privateKeyInfo);

    auto base64 = juce::Base64::toBase64(pkcs8.data(), pkcs8.size());
    juce::String wrapped;
    for (int offset = 0; offset < base64.length(); offset += 64)
        wrapped += base64.substring(offset, offset + 64) + "\n";
    std::fill(sec1.begin(), sec1.end(), 0);
    std::fill(pkcs8.begin(), pkcs8.end(), 0);
    std::fill(privateKeyInfo.begin(), privateKeyInfo.end(), 0);
    return ("-----BEGIN PRIVATE KEY-----\n" + wrapped
        + "-----END PRIVATE KEY-----\n").toStdString();
}

struct SensitiveString
{
    ~SensitiveString() { std::fill(value.begin(), value.end(), '\0'); }
    std::string value;
};
}

ProvenanceService::ProvenanceService()
    : signingProvider(std::make_unique<ConformanceTestSigningProvider>())
{
}

IngredientInfo ProvenanceService::inspect(const juce::File& asset) const
{
    IngredientInfo info;
    if (! asset.existsAsFile())
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.validationSummary = "Asset file is missing";
        return info;
    }

    try
    {
        const auto reader = c2pa::Reader::from_asset(
            makeContext(), std::filesystem::path(asset.getFullPathName().toStdString()));
        if (! reader.has_value())
        {
            info.status = ProvenanceStatus::noCredentials;
            info.validationSummary = "No C2PA manifest is embedded";
            return info;
        }
        return parseManifest(reader->json());
    }
    catch (const std::exception& error)
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.validationSummary = juce::String::fromUTF8(error.what());
        return info;
    }
}

bool ProvenanceService::signingConfigured() const
{
    return signingProvider->configurationError().isEmpty();
}

juce::String ProvenanceService::signingConfigurationError() const
{
    return signingProvider->configurationError();
}

juce::Result ProvenanceService::signWav(
    const juce::File& unsignedWav,
    const juce::File& destination,
    const std::vector<ContributingIngredient>& ingredients,
    const juce::String& outputTitle,
    IngredientInfo& validation) const
{
    if (const auto error = signingConfigurationError(); error.isNotEmpty())
        return juce::Result::fail(error);
    if (! unsignedWav.existsAsFile())
        return juce::Result::fail("Unsigned render is missing");

    const auto pem = signingProvider->credentialFile().loadFileAsString();
    const auto certificate = extractPemBlock(
        pem, "-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----");
    SensitiveString privateKey;
    privateKey.value = extractPemBlock(
        pem, "-----BEGIN PRIVATE KEY-----", "-----END PRIVATE KEY-----");
    if (privateKey.value.empty())
    {
        const auto sec1 = juce::String::fromUTF8(extractPemBlock(
            pem, "-----BEGIN EC PRIVATE KEY-----", "-----END EC PRIVATE KEY-----").c_str());
        if (sec1.isNotEmpty())
            privateKey.value = sec1EcKeyToPkcs8(sec1);
    }
    if (certificate.empty() || privateKey.value.empty())
        return juce::Result::fail("C2PA signing failed: credential bundle is incomplete");

    std::unique_ptr<c2pa::Signer> signer;
    try
    {
        signer = std::make_unique<c2pa::Signer>("es256", certificate, privateKey.value);
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("C2PA signing failed: "
                                  + juce::String::fromUTF8(error.what()));
    }

    std::unique_ptr<c2pa::Builder> builder;
    try
    {
        builder = std::make_unique<c2pa::Builder>(
            makeContext(), makeManifestDefinition(outputTitle).toStdString());
        for (const auto& ingredient : ingredients)
            builder->add_ingredient(makeIngredientDefinition(ingredient).toStdString(),
                std::filesystem::path(ingredient.file.getFullPathName().toStdString()));
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("C2PA manifest creation failed: "
                                  + juce::String::fromUTF8(error.what()));
    }

    try
    {
        builder->sign(
            std::filesystem::path(unsignedWav.getFullPathName().toStdString()),
            std::filesystem::path(destination.getFullPathName().toStdString()), *signer);
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("C2PA embedding failed: "
                                  + juce::String::fromUTF8(error.what()));
    }

    validation = inspect(destination);
    if (! validation.c2paPresent || ! validation.assetIntact)
        return juce::Result::fail(
            "Final C2PA validation failed to confirm intact asset data");
    return juce::Result::ok();
}
}

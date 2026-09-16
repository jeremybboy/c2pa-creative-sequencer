#include "ProvenanceService.h"

#include "app/AppInfo.h"

#include <c2pa.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
    info.retrievalMode = ProvenanceRetrievalMode::embedded;
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

juce::String makeManifestDefinition(const juce::String& title,
                                    const std::optional<SoftBindingClaim>& softBinding)
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
    if (softBinding.has_value())
    {
        auto watermarked = std::make_unique<juce::DynamicObject>();
        watermarked->setProperty("action", "c2pa.watermarked.bound");
        actions.add(watermarked.release());
    }
    auto actionData = std::make_unique<juce::DynamicObject>();
    actionData->setProperty("actions", actions);
    auto assertion = std::make_unique<juce::DynamicObject>();
    assertion->setProperty("label", "c2pa.actions");
    assertion->setProperty("data", actionData.release());
    juce::Array<juce::var> assertions;
    assertions.add(assertion.release());
    if (softBinding.has_value())
    {
        auto timespan = std::make_unique<juce::DynamicObject>();
        timespan->setProperty("start", static_cast<juce::int64>(softBinding->startMilliseconds));
        timespan->setProperty("end", static_cast<juce::int64>(softBinding->endMilliseconds));
        auto scope = std::make_unique<juce::DynamicObject>();
        scope->setProperty("timespan", timespan.release());
        auto block = std::make_unique<juce::DynamicObject>();
        block->setProperty("scope", scope.release());
        // c2pa-rs 0.90.15's official SoftBinding JSON test accepts a base64
        // string here and converts the assertion to its CBOR representation.
        block->setProperty("value", softBinding->payload.toBase64());
        juce::Array<juce::var> blocks;
        blocks.add(block.release());
        auto data = std::make_unique<juce::DynamicObject>();
        data->setProperty("alg", SoftBindingClaim::algorithm);
        data->setProperty("blocks", blocks);
        auto soft = std::make_unique<juce::DynamicObject>();
        soft->setProperty("label", "c2pa.soft-binding");
        soft->setProperty("data", data.release());
        assertions.add(soft.release());
    }
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

std::string extractPemBlocks(const juce::String& bundle,
                             const juce::String& beginMarker,
                             const juce::String& endMarker)
{
    std::string blocks;
    int searchFrom = 0;
    while (searchFrom < bundle.length())
    {
        const auto start = bundle.indexOf(searchFrom, beginMarker);
        if (start < 0)
            break;
        const auto end = bundle.indexOf(start, endMarker);
        if (end < 0)
            break;
        const auto afterEnd = end + endMarker.length();
        blocks += bundle.substring(start, afterEnd).toStdString() + "\n";
        searchFrom = afterEnd;
    }
    return blocks;
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

juce::Result constructSigner(const juce::File& credential,
                             std::unique_ptr<c2pa::Signer>& signer)
{
    if (! credential.existsAsFile())
        return juce::Result::fail("PEM file is not readable.");

    const auto pem = credential.loadFileAsString();
    if (pem.isEmpty())
        return juce::Result::fail("PEM file is empty or unreadable.");

    const auto certificates = extractPemBlocks(
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
    if (certificates.empty())
        return juce::Result::fail("PEM does not contain certificate material.");
    if (privateKey.value.empty())
        return juce::Result::fail("PEM does not contain supported private-key material.");

    try
    {
        signer = std::make_unique<c2pa::Signer>("es256", certificates, privateKey.value);
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("c2pa-cpp could not construct the signer: "
                                  + juce::String::fromUTF8(error.what()));
    }
    return juce::Result::ok();
}
}

ProvenanceService::ProvenanceService(std::unique_ptr<SigningProvider> provider)
    : signingProvider(provider != nullptr
        ? std::move(provider) : std::make_unique<ConformanceTestSigningProvider>())
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
    return signingConfigurationError().isEmpty();
}

juce::String ProvenanceService::signingConfigurationError() const
{
    if (const auto error = signingProvider->configurationError(); error.isNotEmpty())
        return error;
    std::unique_ptr<c2pa::Signer> signer;
    if (const auto validation = constructSigner(signingProvider->credentialFile(), signer);
        validation.failed())
        return "Configured C2PA signing credential is invalid: "
            + validation.getErrorMessage();
    return {};
}

juce::String ProvenanceService::signingCredentialStatus() const
{
    if (const auto error = signingConfigurationError(); error.isNotEmpty())
        return "Not configured\n" + error;
    return signingProvider->statusDescription();
}

juce::Result ProvenanceService::validateSigningCredential(const juce::File& credential) const
{
    std::unique_ptr<c2pa::Signer> signer;
    if (const auto result = constructSigner(credential, signer); result.failed())
        return juce::Result::fail("Selected signing credential is invalid: "
                                  + result.getErrorMessage());
    return juce::Result::ok();
}

juce::Result ProvenanceService::configureSigningCredential(const juce::File& credential)
{
    if (const auto result = validateSigningCredential(credential); result.failed())
        return result;
    if (const auto result = signingProvider->installCredential(credential); result.failed())
        return result;
    if (! signingProvider->isDeveloperOverrideActive())
        if (const auto result = validateSigningCredential(signingProvider->credentialFile());
            result.failed())
            return juce::Result::fail("Stored signing credential failed validation: "
                                      + result.getErrorMessage());
    return juce::Result::ok();
}

juce::Result ProvenanceService::removeSigningCredential()
{
    return signingProvider->removeCredential();
}

juce::Result ProvenanceService::signWav(
    const juce::File& unsignedWav,
    const juce::File& destination,
    const std::vector<ContributingIngredient>& ingredients,
    const juce::String& outputTitle,
    IngredientInfo& validation,
    const std::optional<SoftBindingClaim>& softBinding,
    std::vector<std::uint8_t>* manifestStore) const
{
    if (const auto error = signingConfigurationError(); error.isNotEmpty())
        return juce::Result::fail(error);
    if (! unsignedWav.existsAsFile())
        return juce::Result::fail("Unsigned render is missing");

    std::unique_ptr<c2pa::Signer> signer;
    if (const auto signerResult = constructSigner(signingProvider->credentialFile(), signer);
        signerResult.failed())
        return juce::Result::fail(
            "Audio rendered successfully, but the C2PA claim could not be signed. "
            + signerResult.getErrorMessage());

    std::unique_ptr<c2pa::Builder> builder;
    try
    {
        builder = std::make_unique<c2pa::Builder>(
            makeContext(), makeManifestDefinition(outputTitle, softBinding).toStdString());
        for (const auto& ingredient : ingredients)
            builder->add_ingredient(makeIngredientDefinition(ingredient).toStdString(),
                std::filesystem::path(ingredient.file.getFullPathName().toStdString()));
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("C2PA manifest could not be created: "
                                  + juce::String::fromUTF8(error.what()));
    }

    try
    {
        auto bytes = builder->sign(
            std::filesystem::path(unsignedWav.getFullPathName().toStdString()),
            std::filesystem::path(destination.getFullPathName().toStdString()), *signer);
        if (manifestStore != nullptr)
            manifestStore->assign(bytes.begin(), bytes.end());
    }
    catch (const std::exception& error)
    {
        return juce::Result::fail("C2PA manifest could not be signed and embedded: "
                                  + juce::String::fromUTF8(error.what()));
    }

    validation = inspect(destination);
    if (! validation.c2paPresent || ! validation.assetIntact)
        return juce::Result::fail(
            "Exported Content Credentials failed post-export validation.");
    return juce::Result::ok();
}

IngredientInfo ProvenanceService::inspectRecoveredManifest(
    const juce::File& asset,
    const std::vector<std::uint8_t>& manifestStore) const
{
    IngredientInfo info;
    if (! asset.existsAsFile() || manifestStore.empty())
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.validationSummary = "Recovery asset or stored manifest is missing";
        return info;
    }
    try
    {
        std::ifstream stream(asset.getFullPathName().toStdString(), std::ios::binary);
        c2pa::Reader reader(makeContext(), "wav", stream, manifestStore);
        info = parseManifest(reader.json());
        info.retrievalMode = ProvenanceRetrievalMode::recoveredSoftBinding;
        info.assetIntact = false;
        info.status = ProvenanceStatus::presentWithValidationIssue;
        info.validationSummary = "Recovered signed manifest via WavMark soft binding; "
            "the derivative is not validated by the original hard binding";
        return info;
    }
    catch (const std::exception& error)
    {
        info.status = ProvenanceStatus::unableToValidate;
        info.validationSummary = "Stored manifest could not be inspected: "
            + juce::String::fromUTF8(error.what());
        return info;
    }
}

bool ProvenanceService::hasMatchingSoftBinding(const IngredientInfo& info,
                                               const SoftBindingPayload& payload)
{
    juce::var document;
    if (juce::JSON::parse(info.rawManifestJson, document).failed()) return false;
    auto* root = document.getDynamicObject();
    auto* manifests = root != nullptr
        ? root->getProperty("manifests").getDynamicObject() : nullptr;
    auto* active = manifests != nullptr
        ? manifests->getProperty(info.activeManifest).getDynamicObject() : nullptr;
    auto* assertions = active != nullptr
        ? active->getProperty("assertions").getArray() : nullptr;
    if (assertions == nullptr) return false;
    for (const auto& assertionValue : *assertions)
    {
        auto* assertion = assertionValue.getDynamicObject();
        if (assertion == nullptr
            || ! assertion->getProperty("label").toString().startsWith("c2pa.soft-binding"))
            continue;
        auto* data = assertion->getProperty("data").getDynamicObject();
        auto* blocks = data != nullptr ? data->getProperty("blocks").getArray() : nullptr;
        if (data == nullptr || blocks == nullptr
            || data->getProperty("alg").toString() != SoftBindingClaim::algorithm)
            continue;
        for (const auto& blockValue : *blocks)
            if (auto* block = blockValue.getDynamicObject(); block != nullptr
                && block->getProperty("value").toString() == payload.toBase64())
                return true;
    }
    return false;
}
}

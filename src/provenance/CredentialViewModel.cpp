#include "CredentialViewModel.h"
#include <set>

namespace c2paseq
{
namespace
{
juce::var property(const juce::var& value, const juce::String& key)
{
    return value.getProperty(juce::Identifier(key), {});
}
struct Status { juce::String code, category; };
void statuses(const juce::var& bucket, const juce::String& category, std::vector<Status>& out)
{
    if (const auto* list = bucket.getArray())
        for (const auto& entry : *list)
            if (entry["code"].toString().isNotEmpty()) out.push_back({ entry["code"].toString(), category });
}
juce::String agent(const juce::var& value)
{
    return value.isString() ? value.toString() : value["name"].toString();
}
juce::String generator(const juce::var& manifest)
{
    if (const auto* list = manifest["claim_generator_info"].getArray(); list && ! list->isEmpty())
        return agent(list->getFirst());
    return manifest["claim_generator"].toString();
}
juce::String actionText(const juce::var& action)
{
    auto text = action["action"].toString();
    const auto source = action["digitalSourceType"].toString();
    if (source.isNotEmpty()) text += " | " + source.fromLastOccurrenceOf("/", false, false);
    const auto software = agent(action["softwareAgent"]);
    if (software.isNotEmpty()) text += " | " + software;
    const auto instrument = action["parameters"]["c2paseq:midiRender"]["instrument"]["name"].toString();
    if (instrument.isNotEmpty()) text += " | " + instrument;
    return text;
}
void addActions(CredentialHistoryNode& node, const juce::var& manifest)
{
    if (const auto* assertions = manifest["assertions"].getArray())
        for (const auto& assertion : *assertions)
            if (assertion["label"].toString().startsWith("c2pa.actions"))
                if (const auto* actions = assertion["data"]["actions"].getArray())
                    for (const auto& action : *actions)
                    {
                        if (node.children.size() >= 64)
                        {
                            node.children.push_back({ "Additional actions in Full Details", "Display limit reached", {}, {} });
                            break;
                        }
                        node.children.push_back({ actionText(action), "Declared action",
                                                  juce::JSON::toString(action, true), {}, CredentialNodeKind::action });
                        const auto source = action["digitalSourceType"].toString();
                        if (source.isNotEmpty()) node.declaredSources.addIfNotAlreadyThere(source);
                        const auto instrument = action["parameters"]["c2paseq:midiRender"]["instrument"]["name"].toString();
                        if (instrument.isNotEmpty()) node.instrument = instrument;
                    }
}
CredentialHistoryNode history(const juce::var& document, const juce::String& id,
    const juce::String& title, const std::vector<ContributingIngredient>& local,
    std::set<juce::String> path, int depth, int& budget)
{
    CredentialHistoryNode node { title, "Embedded manifest history", {}, {} };
    if (--budget <= 0 || depth > 8 || path.contains(id))
    { node.subtitle = "History truncated (size, depth or repeated reference)"; return node; }
    path.insert(id);
    const auto manifest = property(document["manifests"], id);
    node.details = "Manifest: " + id + "\n\n" + juce::JSON::toString(manifest, true);
    const auto tool = generator(manifest);
    node.generator = tool;
    node.embeddedManifest = true;
    if (tool.isNotEmpty()) node.subtitle = "Claim generator: " + tool;
    addActions(node, manifest);
    if (const auto* ingredients = manifest["ingredients"].getArray())
        for (const auto& ingredient : *ingredients)
        {
            if (budget <= 0) break;
            const auto linkedId = ingredient["active_manifest"].toString();
            const auto name = ingredient["title"].toString();
            CredentialHistoryNode child { name.isNotEmpty() ? name : "Untitled ingredient", {}, {}, {} };
            child.kind = CredentialNodeKind::ingredient;
            child.relationship = ingredient["relationship"].toString();
            child.referencedManifest = linkedId;
            const auto found = std::find_if(local.begin(), local.end(), [&](const auto& item)
            {
                // Never match identity by filename alone.
                return linkedId.isNotEmpty() && item.provenance.activeManifest == linkedId;
            });
            const bool available = found != local.end();
            child.sourceAvailable = available;
            child.subtitle = available ? "Local source; cached inspection (not revalidated here)"
                : "Referenced ingredient; source unavailable for independent validation";
            if (linkedId.isEmpty())
            {
                child.credentialsAbsent = true;
                child.subtitle = "No Content Credentials in declared ingredient; source not independently validated";
            }
            child.details = "Relationship: " + ingredient["relationship"].toString()
                + "\n" + child.subtitle + "\n\n" + juce::JSON::toString(ingredient, true);
            if (linkedId.isNotEmpty() && property(document["manifests"], linkedId).isObject())
                child.children.push_back(history(document, linkedId, "Available earlier provenance",
                                                 local, path, depth + 1, budget));
            else if (linkedId.isNotEmpty())
                child.children.push_back({ "Earlier manifest not embedded", linkedId, {}, {}, CredentialNodeKind::notice });
            node.children.push_back(std::move(child));
            --budget;
        }
    return node;
}
}

CredentialViewModel makeCredentialViewModel(const IngredientInfo& info, const juce::String& title,
                                           const std::vector<ContributingIngredient>& local)
{
    CredentialViewModel view;
    view.title = title;
    view.checks = {{{ CredentialCheckState::unknown, "Content binding", "Not separately reported", {} },
                    { CredentialCheckState::unknown, "Claim signature", "Not separately reported", {} },
                    { CredentialCheckState::unknown, "Signer trust", "Not evaluated / not reported", {} }}};
    view.policy = "Policy: cached SDK inspection; trust anchors not reported. No official-list or test-trust assertion.";
    if (info.inspectionPolicy.isNotEmpty()) view.policy = "Policy: " + info.inspectionPolicy
        + ". No official-list or test-trust assertion.";
    view.badge = "CC ?";
    view.description = "Content Credentials are present; review the reported results below.";
    view.details = "File: " + title + "\nActive manifest: " + info.activeManifest
        + "\nClaim generator: " + info.claimGenerator + "\nSigner: " + info.signer
        + "\nInspection summary: " + info.validationSummary + "\n" + view.policy;
    if (! info.c2paPresent)
    {
        const bool unable = info.status == ProvenanceStatus::unableToValidate;
        view.badge = unable ? "CC ?" : "No CC";
        view.description = unable ? "Unable to inspect this asset; credential presence is unknown."
            : "No Content Credentials are embedded in this file. This is not a validation failure.";
        for (auto& check : view.checks)
        { check.state = unable ? CredentialCheckState::unknown : CredentialCheckState::notApplicable;
          check.summary = unable ? "Unable to determine" : "Not applicable - no manifest"; }
        view.history = { title, unable ? "Inspection unavailable" : "No embedded provenance history", {}, {} };
        view.history.credentialsAbsent = ! unable;
        view.details += "\n" + view.description;
        return view;
    }
    const auto document = juce::JSON::parse(info.rawManifestJson);
    const auto activeId = document["active_manifest"].toString();
    const auto manifest = property(document["manifests"], activeId);
    if (activeId.isEmpty() || ! manifest.isObject())
    {
        view.description = "Content Credentials detected, but no readable active manifest is available.";
        view.history = { title, "Manifest unavailable", {}, {} };
        view.details += "\n\nRaw manifest JSON:\n" + info.rawManifestJson;
        return view;
    }
    std::vector<Status> results;
    const auto active = document["validation_results"]["activeManifest"];
    for (const auto* category : { "success", "informational", "failure" })
        statuses(active[juce::Identifier(category)], category, results);
    // Legacy statuses are scoped to the active asset, not recursively gathered from ingredients.
    if (const auto* legacy = document["validation_status"].getArray())
        for (const auto& entry : *legacy)
        {
            const auto url = entry["url"].toString();
            if (url.isEmpty() || url.contains("/" + activeId + "/"))
                results.push_back({ entry["code"].toString(), "legacy" });
        }
    auto apply = [&](int index, CredentialCheckState state, const juce::String& summary)
    {
        auto& check = view.checks[static_cast<size_t>(index)];
        if (check.state != CredentialCheckState::fail)
        { check.state = state; check.summary = summary; }
    };
    juce::String allCodes;
    bool issue = false;
    bool informational = false;
    for (const auto& result : results)
    {
        const auto& code = result.code;
        const auto line = result.category + ": " + code + "\n";
        allCodes += line;
        const bool failure = result.category == "failure";
        issue |= failure || result.category == "informational" || result.category == "legacy";
        informational |= result.category == "informational";
        const bool binding = code.startsWith("assertion.dataHash.") || code.startsWith("assertion.bmffHash.")
            || code.startsWith("assertion.boxesHash.") || code.startsWith("assertion.collectionHash.");
        if (binding)
        {
            view.checks[0].evidence += line;
            if (code.endsWith(".mismatch") || failure) apply(0, CredentialCheckState::fail, "Audio does not match its manifest");
            else if (code.endsWith(".match")) apply(0, CredentialCheckState::pass, "Audio matches the reported hard binding");
        }
        if (code.startsWith("claimSignature."))
        {
            view.checks[1].evidence += line;
            if (failure || code == "claimSignature.mismatch" || code == "claimSignature.missing"
                || code == "claimSignature.invalid")
                apply(1, CredentialCheckState::fail, "Signature verification failed");
            else if (code == "claimSignature.validated") apply(1, CredentialCheckState::pass, "Signature is valid");
        }
        if (code.startsWith("signingCredential."))
        {
            view.checks[2].evidence += line;
            if (code == "signingCredential.trusted" && ! failure)
                apply(2, CredentialCheckState::pass, "Trusted under the inspection policy");
            else if (code == "signingCredential.ocsp.inaccessible" || (code.contains("ocsp") && ! failure
                     && ! code.contains("revoked")))
            {
                if (view.checks[2].state != CredentialCheckState::pass)
                    apply(2, CredentialCheckState::warning, "Certificate status warning; trust unresolved");
            }
            else if (failure || code == "signingCredential.untrusted" || code == "signingCredential.invalid"
                     || code == "signingCredential.revoked" || code == "signingCredential.expired")
                apply(2, CredentialCheckState::fail, "Not trusted / credential validation failed");
        }
    }
    if (! issue && info.validationIssues.empty() && std::all_of(view.checks.begin(), view.checks.end(),
        [](const auto& check) { return check.state == CredentialCheckState::pass; }))
        view.badge = juce::String::fromUTF8("CC \xe2\x9c\x93");
    int budget = 128;
    if (informational) view.policy += " Additional informational statuses reported - see Details.";
    view.history = history(document, activeId, title, local, {}, 0, budget);
    view.details += "\n\nActive manifest results:\n" + allCodes;
    for (const auto& code : info.validationIssues) view.details += "\nSDK issue: " + code;
    view.details += "\n\nRaw manifest JSON (actions, parameters, relationships and embedded history):\n"
        + juce::JSON::toString(document, true);
    bool capture = false, render = false;
    for (const auto& node : view.history.children)
    { capture |= node.title.contains("digitalCapture"); render |= node.title.startsWith("c2pa.rendered"); }
    if (capture) view.description = "The manifest declares digital capture; tool: " + generator(manifest) + ".";
    else if (render) view.description = "The manifest declares rendered audio; expand the history for MIDI/instrument details.";
    else view.description = "This file has existing Content Credentials; declared history is shown without rewriting its origin.";
    return view;
}

CredentialViewModel makeUnrenderedMidiCredentialView(const juce::String& title)
{
    auto view = makeCredentialViewModel({}, title);
    view.badge = "CC on export";
    view.description = "Project MIDI is not a signed audio asset. Eligible audible MIDI/instrument material receives provenance during signed Export.";
    view.history = { title, "Project MIDI - no persistent signed audio asset", {}, {} };
    view.details = view.description;
    return view;
}
}

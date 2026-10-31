#include "provenance/CredentialViewModel.h"
#include "provenance/TestTrust.h"
#include "ui/CredentialsPanel.h"
#include "ui/SequencerLookAndFeel.h"
#include <juce_graphics/juce_graphics.h>
#include <c2pa.hpp>
#include <filesystem>
#include <iostream>

using namespace c2paseq;
namespace
{
IngredientInfo fixture(const juce::String& success, const juce::String& failure = {},
                       const juce::String& informational = {})
{
    auto codes = [](const juce::String& input)
    {
        juce::Array<juce::var> list;
        for (const auto& code : juce::StringArray::fromTokens(input, ",", ""))
        {
            if (code.isEmpty()) continue;
            auto* object = new juce::DynamicObject;
            object->setProperty("code", code); list.add(object);
        }
        return juce::var(list);
    };
    auto document = juce::JSON::parse(R"({"active_manifest":"root","manifests":{"root":{
        "title":"Mixed song.wav","claim_generator_info":[{"name":"C2PA Creative Sequencer"}],
        "assertions":[{"label":"c2pa.actions.v2","data":{"actions":[
          {"action":"c2pa.created","digitalSourceType":"http://cv.iptc.org/newscodes/digitalsourcetype/digitalCapture","softwareAgent":{"name":"C2PA Creative Sequencer"}}]}}],
        "ingredients":[{"title":"Unsigned sample","relationship":"componentOf"},
         {"title":"Generated loop","relationship":"componentOf","active_manifest":"external"},
         {"title":"Unavailable source","relationship":"componentOf","active_manifest":"absent"}]},
        "external":{"claim_generator":"External Music Tool","assertions":[{"label":"c2pa.actions.v2","data":{"actions":[{"action":"c2pa.created","digitalSourceType":"http://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia"}]}}]}}})");
    auto* bucket = new juce::DynamicObject;
    bucket->setProperty("success", codes(success)); bucket->setProperty("failure", codes(failure));
    bucket->setProperty("informational", codes(informational));
    auto* results = new juce::DynamicObject; results->setProperty("activeManifest", bucket);
    document.getDynamicObject()->setProperty("validation_results", results);
    IngredientInfo info;
    info.c2paPresent = true; info.assetIntact = true; info.activeManifest = "root";
    info.claimGenerator = "C2PA Creative Sequencer";
    info.rawManifestJson = juce::JSON::toString(document);
    return info;
}
void require(bool value, const char* message)
{ if (! value) { std::cerr << message << '\n'; std::exit(1); } }
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto validCodes = "assertion.dataHash.match,claimSignature.validated,signingCredential.trusted";
    auto plain = makeCredentialViewModel({}, "Unsigned.wav");
    require(plain.badge == "No CC" && plain.history.children.empty(), "unsigned history invented");
    for (const auto& check : plain.checks) require(check.state == CredentialCheckState::notApplicable, "unsigned green card");
    auto good = fixture(validCodes);
    auto view = makeCredentialViewModel(good, "Mixed song.wav");
    require(view.badge != "CC ?" && view.checks[0].state == CredentialCheckState::pass, "binding success missing");
    require(view.history.subtitle.contains("C2PA Creative Sequencer") && view.description.contains("digital capture"), "capture evidence not shown");
    require(view.history.children.size() == 4, "declared ingredients/actions missing");
    require(view.history.children[1].subtitle.contains("No Content Credentials"), "unsigned ingredient not represented");
    require(view.history.children[2].children[0].subtitle.contains("External Music Tool"), "external history overwritten");
    require(view.history.children[3].subtitle.contains("unavailable"), "unavailable ingredient not explicit");
    auto bad = makeCredentialViewModel(fixture("claimSignature.validated", "assertion.dataHash.mismatch"), "Broken.wav");
    require(bad.checks[0].state == CredentialCheckState::fail && bad.checks[1].state == CredentialCheckState::pass, "binding/signature conflated");
    auto sig = makeCredentialViewModel(fixture("", "claimSignature.mismatch"), "Broken signature.wav");
    require(sig.checks[1].state == CredentialCheckState::fail, "signature failure missed");
    auto unknown = makeCredentialViewModel(fixture(""), "Unknown.wav");
    for (const auto& check : unknown.checks) require(check.state == CredentialCheckState::unknown, "missing evidence fabricated success");
    auto warning = makeCredentialViewModel(fixture(validCodes, {}, "signingCredential.ocsp.inaccessible"), "Warning.wav");
    auto demoFixture = good; demoFixture.demoTrustEvaluation = true;
    demoFixture.inspectionPolicy = "DEMO/TEST fixture policy; not production trust";
    const auto demoView = makeCredentialViewModel(demoFixture, "Demo trust.wav");
    require(demoView.demoTrust && demoView.checks[2].state == CredentialCheckState::pass
        && demoView.checks[2].summary.contains("test certificate used") && demoView.badge.contains("Test Mode"), "test mode label missing");
    auto demoFailure = fixture("signingCredential.trusted", "claimSignature.mismatch");
    demoFailure.demoTrustEvaluation = true;
    require(makeCredentialViewModel(demoFailure, "Bad demo").checks[1].state == CredentialCheckState::fail,
            "demo mode hid signature failure");
    require(warning.checks[2].state == CredentialCheckState::pass && warning.checks[2].evidence.contains("ocsp"), "OCSP overwrote explicit trust");
    require(warning.badge == "CC ?", "warning erased from overall badge");
    auto untrusted = makeCredentialViewModel(fixture("assertion.dataHash.match,claimSignature.validated", "signingCredential.untrusted"), "Untrusted.wav");
    require(untrusted.checks[2].state == CredentialCheckState::fail && untrusted.checks[1].state == CredentialCheckState::pass, "signature treated as trust");
    auto inaccessible = makeCredentialViewModel(fixture("", {}, "signingCredential.ocsp.inaccessible"), "OCSP.wav");
    require(inaccessible.checks[2].state == CredentialCheckState::warning, "OCSP warning lost");
    auto midi = makeUnrenderedMidiCredentialView("MIDI clip");
    require(midi.badge == "CC on export" && midi.history.children.empty(), "MIDI falsely signed");
    auto renderInfo = good;
    renderInfo.rawManifestJson = renderInfo.rawManifestJson.replace("digitalCapture", "digitalCreation")
        .replace("c2pa.created", "c2pa.rendered");
    auto render = makeCredentialViewModel(renderInfo, "MIDI Render.wav");
    require(render.description.contains("rendered audio"), "render action missing");
    // Ingredient-only successes/failures must not bleed into the active asset's cards.
    auto scoped = good;
    auto document = juce::JSON::parse(scoped.rawManifestJson);
    auto* ingredientResults = new juce::DynamicObject;
    ingredientResults->setProperty("code", "assertion.dataHash.mismatch");
    document.getDynamicObject()->setProperty("ingredientOnlyTestStatus", ingredientResults);
    scoped.rawManifestJson = juce::JSON::toString(document);
    require(makeCredentialViewModel(scoped, "Scoped").checks[0].state == CredentialCheckState::pass, "recursive status leakage");
    auto unavailable = good; unavailable.rawManifestJson = "{}";
    require(makeCredentialViewModel(unavailable, "Unreadable").badge == "CC ?", "unreadable manifest green");
    IngredientInfo failed; failed.status = ProvenanceStatus::unableToValidate;
    require(makeCredentialViewModel(failed, "Missing").badge == "CC ?", "inspection failure misrepresented as absence");

    // A spatial graph of real contributions, not one asset node per declared action.
    const auto graph = makeProvenanceGraph(view.history);
    require(graph.children.size() == 3 && graph.actions.size() == 1, "graph fabricated action assets");
    require(graph.children[1].generator == "External Music Tool"
        && graph.children[1].sources.contains("Contains GenAI Media"), "graph lost declared external origin");
    require(graph.sources.contains("Contains Captured Media"), "capture label not clear");
    auto labelNode = view.history;
    labelNode.children.clear();
    labelNode.declaredSources = { "https://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation" };
    require(makeProvenanceGraph(labelNode).sources.contains("Contains Non-GenAI digital creation"), "digital creation label missing");
    labelNode.declaredSources = { "http://cv.iptc.org/newscodes/digitalsourcetype/compositeWithTrainedAlgorithmicMedia" };
    require(makeProvenanceGraph(labelNode).sources.contains("Contains GenAI edits"), "GenAI edit overstated as full generation");
    labelNode.declaredSources = { "https://example.test/trainedAlgorithmicMedia" };
    require(! makeProvenanceGraph(labelNode).sources.contains("Contains GenAI Media"), "foreign vocabulary inferred as IPTC GenAI");
    require(graph.children[1].hasManifest && graph.children[1].sourceUnavailable,
            "embedded manifest conflated with available source audio");
    require(! graph.children[2].hasManifest && graph.children[2].hasManifestReference, "reference treated as embedded credentials");
    const auto placed = layoutProvenanceGraph(graph, {});
    for (size_t i = 0; i < placed.size(); ++i)
        for (size_t j = i + 1; j < placed.size(); ++j)
            require(! placed[i].bounds.intersects(placed[j].bounds), "graph nodes overlap");
    const auto folded = layoutProvenanceGraph(graph, { "root" });
    require(folded.size() == 1 && folded.front().parent == -1, "root collapse lost root or retained branches");
    ProvenanceGraphCamera camera; camera.translation = { 27, -45 };
    const juce::Point<float> anchor(234, 161);
    const auto before = camera.toWorld(anchor); camera.zoomAt(1.8f, anchor);
    require(camera.toScreen(before).getDistanceFrom(anchor) < 0.01f, "graph zoom anchor drifted");
    camera.zoomAt(1.0e20f, anchor); require(camera.zoom == 4.0f, "zoom upper bound failed");
    camera.zoomAt(1.0e-20f, anchor); require(camera.zoom == 0.08f, "zoom lower bound failed");
    juce::Rectangle<float> whole;
    for (const auto& item : placed) whole = whole.getUnion(item.bounds);
    const juce::Rectangle<float> viewport(0, 0, 1000, 600);
    camera.fit(whole, viewport);
    require(viewport.expanded(0.1f).contains({ camera.toScreen(whole.getTopLeft()),
                                            camera.toScreen(whole.getBottomRight()) }), "fit-to-view misses nodes");
    ProvenanceGraphView explorer; explorer.setSize(1440, 650); explorer.setHistory(view.history);
    explorer.expandAll(); const auto expandedCount = explorer.placements().size();
    explorer.toggleBranch("root"); require(explorer.placements().size() == 1, "interactive collapse failed");
    explorer.toggleBranch("root"); require(explorer.placements().size() == expandedCount, "interactive expansion failed");
    explorer.selectNode(explorer.placements()[1].node->id);
    bool matchedDetail = false;
    for (auto* child : explorer.getChildren())
        if (auto* editor = dynamic_cast<juce::TextEditor*>(child)) matchedDetail |= editor->getText().contains("Unsigned sample");
    require(matchedDetail, "node selection did not update corresponding details");

    CredentialsPanel dock; dock.setSize(1040, 300); dock.setClip(view, "Track");
    require(! dock.isDemoTrustEnabled(), "demo mode unexpectedly enabled by default");
    int modeChanges = 0; dock.onDemoTrustChanged = [&](bool) { ++modeChanges; };
    dock.setDemoTrustEnabled(true); dock.setDemoTrustEnabled(false);
    require(! dock.isDemoTrustEnabled() && modeChanges == 2, "demo toggle did not isolate session mode");
    int expansionChanges = 0; dock.onExpansionChanged = [&] { ++expansionChanges; };
    dock.setExpanded(true); require(dock.isExpanded() && expansionChanges == 1, "expanded workspace did not request layout");
    dock.setExpanded(false); require(! dock.isExpanded() && expansionChanges == 2, "return to arrangement failed");

    // The actual pinned Reader is authoritative for field names and real-file evidence.
    const auto reader = c2pa::Reader::from_asset(std::make_shared<c2pa::Context>(),
                                               std::filesystem::path(C2PA_KNOWN_SIGNED_WAV));
    require(reader.has_value(), "pinned signed fixture cannot be read");
    IngredientInfo actual; actual.c2paPresent = true; actual.rawManifestJson = reader->json();
    actual.activeManifest = juce::JSON::parse(actual.rawManifestJson)["active_manifest"].toString();
    const auto real = makeCredentialViewModel(actual, "SDK signed fixture.wav");
    require(real.checks[0].state == CredentialCheckState::pass && real.checks[1].state == CredentialCheckState::pass,
            "actual pinned SDK success format differs");
    require(real.checks[2].state != CredentialCheckState::pass, "default SDK fixture unexpectedly asserted trusted");

    if ((argc == 4 && juce::String(argv[1]) == "--render-asset")
        || (argc == 4 && juce::String(argv[1]) == "--render-test-asset")
        || (argc == 5 && juce::String(argv[1]) == "--render-demo-asset"))
    {
        const bool builtInTest = juce::String(argv[1]) == "--render-test-asset";
        const bool demo = argc == 5 || builtInTest;
        auto context = std::make_shared<c2pa::Context>();
        if (demo)
        {
            const auto publicCertificates = builtInTest ? juce::String(testTrust::rootCertificate)
                : juce::File(argv[3]).loadFileAsString();
            require(! publicCertificates.contains("PRIVATE KEY"), "demo render accepts public certificates only");
            auto settings = juce::JSON::parse(R"({"verify":{"remote_manifest_fetch":false,"ocsp_fetch":false},"trust":{}})");
            settings["trust"].getDynamicObject()->setProperty("user_anchors", publicCertificates);
            context = std::make_shared<c2pa::Context>(juce::JSON::toString(settings).toStdString());
        }
        const auto outputPath = juce::File(argv[argc == 5 ? 4 : 3]);
        const auto assetReader = c2pa::Reader::from_asset(context,
                                                        std::filesystem::path(argv[2]));
        require(assetReader.has_value(), "render asset has no manifest");
        IngredientInfo data; data.c2paPresent = true; data.rawManifestJson = assetReader->json();
        data.demoTrustEvaluation = demo;
        if (demo) data.inspectionPolicy = "Test Mode: explicit local test certificates; offline; not production trust";
        auto model = makeCredentialViewModel(data, juce::File(argv[2]).getFileName());
        if (demo) require(model.checks[2].state == CredentialCheckState::pass, "actual demo render did not report trusted");
        SequencerLookAndFeel look; CredentialsPanel panel; panel.setLookAndFeel(&look);
        panel.setVisible(true); panel.setSize(1440, 320); panel.setClip(model, "Local signed asset");
        panel.setDemoTrustEnabled(demo);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        juce::FileOutputStream output { outputPath };
        output.setPosition(0); require(output.truncate().wasOk(), "asset render reset failed");
        require(juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), output), "asset render failed");
        panel.setLookAndFeel(nullptr);
        panel.setLookAndFeel(&look); panel.setSize(1440, 760); panel.setExpanded(true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        juce::FileOutputStream expandedOutput { outputPath.getSiblingFile("graph-real-mix.png") };
        expandedOutput.setPosition(0); expandedOutput.truncate();
        require(juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), expandedOutput), "expanded asset render failed");
        panel.setLookAndFeel(nullptr);
    }
    if (argc == 3 && juce::String(argv[1]) == "--render")
    {
        require(juce::File(argv[2]).createDirectory().wasOk(), "graph render directory failed");
        // Four-contribution layout fixture; not an execution or production-trust claim.
        auto mixDocument = juce::JSON::parse(good.rawManifestJson);
        auto& mix = *mixDocument["manifests"].getDynamicObject();
        auto rootManifest = mix.getProperty("root");
        rootManifest.getDynamicObject()->setProperty("ingredients", juce::JSON::parse(R"([
            {"title":"Drums - unsigned sample","relationship":"componentOf"},
            {"title":"Generated loop","relationship":"componentOf","active_manifest":"external"},
            {"title":"Vocal - mic recording","relationship":"componentOf","active_manifest":"capture"},
            {"title":"MIDI instrument stem","relationship":"componentOf","active_manifest":"synth"}])"));
        auto captureManifest = juce::JSON::parse(good.rawManifestJson)["manifests"]["root"];
        captureManifest.getDynamicObject()->removeProperty("ingredients");
        mix.setProperty("capture", captureManifest);
        mix.setProperty("synth", juce::JSON::parse(R"json({"claim_generator":"C2PA Creative Sequencer",
          "assertions":[{"label":"c2pa.actions.v2","data":{"actions":[{"action":"c2pa.rendered",
          "digitalSourceType":"http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation",
          "parameters":{"c2paseq:midiRender":{"instrument":{"name":"Surge XT (fixture)"}}}}]}}]})json"));
        rootManifest["assertions"][0]["data"]["actions"][0].getDynamicObject()->setProperty("digitalSourceType",
            "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation");
        auto graphInfo = good; graphInfo.rawManifestJson = juce::JSON::toString(mixDocument);
        const auto graphView = makeCredentialViewModel(graphInfo, "Final mixed song.wav");
        for (const auto width : { 1040, 1440 })
        {
            SequencerLookAndFeel look; CredentialsPanel panel; panel.setLookAndFeel(&look);
            panel.setVisible(true); panel.setSize(width, 760); panel.setLastExport(graphView); panel.showLastExport();
            panel.setExpanded(true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            for (auto* child : panel.getChildren())
                require(! child->isVisible() || panel.getLocalBounds().contains(child->getBounds()), "expanded UI outside bounds");
            const auto target = juce::File(argv[2]).getChildFile("graph-" + juce::String(width) + ".png");
            juce::FileOutputStream output(target); output.setPosition(0); output.truncate();
            require(juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), output), "graph render failed");
            panel.setLookAndFeel(nullptr);
        }
    }
    if (argc == 3 && juce::String(argv[1]) == "--render")
    {
        SequencerLookAndFeel look;
        const std::vector<std::pair<juce::String, CredentialViewModel>> states {
            {"no-cc", plain}, {"valid", view}, {"warning", warning},
            {"recording", makeCredentialViewModel(good, "Microphone take.wav")},
            {"midi-render", render}, {"export", view}, {"real-sdk", real}, {"demo-trust", demoView} };
        for (const auto& [name, model] : states)
            for (const auto width : { 1040, 1440 })
            {
                CredentialsPanel panel; panel.setLookAndFeel(&look); panel.setVisible(true); panel.setSize(width, 300);
                panel.setClip(model, "Selected track");
                if (name == "export") { panel.setLastExport(model); panel.showLastExport(); }
                if (name == "demo-trust") panel.setDemoTrustEnabled(true);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
                for (auto* child : panel.getChildren())
                    require(! child->isVisible() || panel.getLocalBounds().contains(child->getBounds()), "panel child outside bounds");
                const auto target = juce::File(argv[2]).getChildFile(name + "-" + juce::String(width) + ".png");
                require(target.getParentDirectory().createDirectory().wasOk(), "render directory failed");
                juce::FileOutputStream stream(target);
                stream.setPosition(0); require(stream.truncate().wasOk(), "render reset failed");
                require(juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), stream), "render failed");
                panel.setLookAndFeel(nullptr);
            }
    }
    std::cout << "credential viewer: explicit/scoped SDK evidence, neutral unknowns, trust warnings, declared history, MIDI and real Reader passed\n";
}

#include "provenance/CredentialViewModel.h"
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

    if (argc == 4 && juce::String(argv[1]) == "--render-asset")
    {
        const auto assetReader = c2pa::Reader::from_asset(std::make_shared<c2pa::Context>(),
                                                        std::filesystem::path(argv[2]));
        require(assetReader.has_value(), "render asset has no manifest");
        IngredientInfo data; data.c2paPresent = true; data.rawManifestJson = assetReader->json();
        auto model = makeCredentialViewModel(data, juce::File(argv[2]).getFileName());
        SequencerLookAndFeel look; CredentialsPanel panel; panel.setLookAndFeel(&look);
        panel.setVisible(true); panel.setSize(1440, 320); panel.setLastExport(model); panel.showLastExport();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(80);
        juce::FileOutputStream output { juce::File(argv[3]) };
        output.setPosition(0); require(output.truncate().wasOk(), "asset render reset failed");
        require(juce::PNGImageFormat().writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), output), "asset render failed");
        panel.setLookAndFeel(nullptr);
    }
    if (argc == 3 && juce::String(argv[1]) == "--render")
    {
        SequencerLookAndFeel look;
        const std::vector<std::pair<juce::String, CredentialViewModel>> states {
            {"no-cc", plain}, {"valid", view}, {"warning", warning},
            {"recording", makeCredentialViewModel(good, "Microphone take.wav")},
            {"midi-render", render}, {"export", view}, {"real-sdk", real} };
        for (const auto& [name, model] : states)
            for (const auto width : { 1040, 1440 })
            {
                CredentialsPanel panel; panel.setLookAndFeel(&look); panel.setVisible(true); panel.setSize(width, 300);
                panel.setClip(model, "Selected track");
                if (name == "export") { panel.setLastExport(model); panel.showLastExport(); }
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

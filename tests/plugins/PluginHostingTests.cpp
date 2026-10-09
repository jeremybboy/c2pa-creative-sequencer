#include "engine/AudioEngine.h"
#include "engine/ProjectEngine.h"
#include "engine/TracktionAdapter.h"
#include "export/RenderService.h"
#include "plugins/PluginHost.h"
#include "plugins/PluginScanner.h"
#include "provenance/ProvenanceService.h"

#include <cmath>
#include <juce_cryptography/juce_cryptography.h>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>

namespace
{
int fail(int code, const juce::String& message)
{
    std::cerr << message << '\n';
    return code;
}

bool writeTone(const juce::File& file)
{
    constexpr double sampleRate = 48000.0;
    constexpr int samples = 48000;
    juce::AudioBuffer<float> source(2, samples);
    for (int channel = 0; channel < source.getNumChannels(); ++channel)
        for (int sample = 0; sample < samples; ++sample)
            source.setSample(channel, sample, 0.4f * std::sin(
                juce::MathConstants<double>::twoPi * 220.0 * sample / sampleRate));
    auto stream = file.createOutputStream();
    juce::WavAudioFormat format;
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 2, 24, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer(source, 0, samples);
}

double readRms(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (reader == nullptr)
        return 0.0;
    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels),
                                   static_cast<int>(reader->lengthInSamples));
    if (! reader->read(&audio, 0, audio.getNumSamples(), 0, true, true))
        return 0.0;
    return audio.getRMSLevel(0, 0, audio.getNumSamples());
}

bool pointSavedPluginAtMissingBundle(const juce::File& projectFolder)
{
    const auto projectJson = projectFolder.getChildFile("project.json");
    auto stream = projectJson.createInputStream();
    if (stream == nullptr)
        return false;
    juce::var document;
    if (juce::JSON::parse(stream->readEntireStreamAsString(), document).failed())
        return false;
    auto* root = document.getDynamicObject();
    auto* plugins = root != nullptr ? root->getProperty("plugins").getArray() : nullptr;
    if (plugins == nullptr || plugins->isEmpty())
        return false;
    auto* plugin = plugins->getReference(0).getDynamicObject();
    if (plugin == nullptr)
        return false;
    plugin->setProperty("fileOrIdentifier", "/missing/C2PA Test Gain.vst3");
    plugin->setProperty("missing", false);
    return projectJson.replaceWithText(juce::JSON::toString(document, true));
}

juce::String savedPluginState(const juce::File& projectFolder)
{
    auto stream = projectFolder.getChildFile("project.json").createInputStream();
    if (stream == nullptr)
        return {};
    juce::var document;
    if (juce::JSON::parse(stream->readEntireStreamAsString(), document).failed())
        return {};
    auto* root = document.getDynamicObject();
    auto* plugins = root != nullptr ? root->getProperty("plugins").getArray() : nullptr;
    if (plugins == nullptr || plugins->isEmpty())
        return {};
    auto* plugin = plugins->getReference(0).getDynamicObject();
    return plugin != nullptr ? plugin->getProperty("stateBase64").toString() : juce::String {};
}

juce::var stemActions(const c2paseq::IngredientInfo& info)
{
    const auto document = juce::JSON::parse(info.rawManifestJson);
    const auto manifest = document["manifests"][juce::Identifier(info.activeManifest)];
    if (const auto* assertions = manifest["assertions"].getArray())
        for (const auto& assertion : *assertions)
            if (assertion["label"].toString().startsWith("c2pa.actions"))
                return assertion["data"]["actions"];
    return {};
}

juce::Result exerciseMidiStemBounce(const c2paseq::PluginDescriptor& synth,
                                     const juce::File& source, const juce::File& root,
                                     const juce::File& cache)
{
    const auto folder = root.getChildFile("stem-bounce.c2paseq");
    const auto bundle = root.getChildFile("stem-test-signing.pem");
    if (! bundle.replaceWithText(juce::File(C2PA_TEST_CERTIFICATE_PEM).loadFileAsString()
            + "\n" + juce::File(C2PA_TEST_PRIVATE_KEY_PEM).loadFileAsString()))
        return juce::Result::fail("could not assemble stem signing fixture");
    auto provider = std::make_unique<c2paseq::ConformanceTestSigningProvider>(
        root.getChildFile("stem-signing-config"), false);
    c2paseq::AudioEngine engine(std::move(provider), cache, false, {},
                                root.getChildFile("stem-outbox"));
    c2paseq::MidiStemPlan plan;
    if (engine.prepareMidiStem(0, plan).wasOk())
        return juce::Result::fail("no-project bounce was accepted");
    if (engine.createProject(folder, "Stem acceptance").failed()
        || engine.addMidiTrack().failed())
        return juce::Result::fail("could not prepare stem project");
    if (engine.prepareMidiStem(4, plan).wasOk()
        || engine.configureSigningCredential(bundle).failed()
        || engine.prepareMidiStem(0, plan).wasOk()
        || engine.prepareMidiStem(4, plan).wasOk())
        return juce::Result::fail("invalid target or empty/instrumentless MIDI was accepted");
    if (engine.createMidiClip(4, 4.0, 4.0).failed())
        return juce::Result::fail("could not add MIDI bounce clip");
    auto tracks = engine.arrangementSnapshot();
    const auto clipId = tracks[4].midiClips.front().id;
    if (engine.addMidiNote(clipId, 60, 0.0, 1.0, 100).failed()
        || engine.prepareMidiStem(4, plan).wasOk()
        || engine.loadTrackPlugin(4, synth.identifier).failed()
        || engine.setTrackName(4, "Actual Instrument Track").failed()
        || engine.setTrackGain(4, -6.0).failed()
        || engine.setTrackPan(4, -0.25).failed()
        || engine.importAudio(source, 0, 2.0).failed()
        || engine.setTrackSolo(0, true).failed()
        || engine.setTrackMute(4, true).failed())
        return juce::Result::fail("could not prepare real MIDI/instrument bounce state");
    // An unrelated solo and a muted source must not silence or contaminate a stem.
    engine.seek(1.25);
    if (auto result = engine.prepareMidiStem(4, plan); result.failed()) return result;
    if (std::abs(plan.startSeconds - 2.0) > 0.001
        || std::abs(plan.endSeconds - 4.0) > 0.001)
        return juce::Result::fail("stem range lost beat-to-time alignment");
    const auto originalPlan = plan;
    const auto unsignedWav = root.getChildFile("unsigned-stem.wav");
    const auto signedWav = root.getChildFile(plan.descriptor.title);
    if (auto result = engine.beginMidiStemRender(plan); result.failed()) return result;
    auto rendered = std::async(std::launch::async, [&]
    { return engine.renderAndSignMidiStem(plan, unsignedWav, signedWav); });
    while (rendered.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    const auto renderResult = rendered.get();
    engine.finishMidiStemRender();
    if (renderResult.failed()) return renderResult;
    if (readRms(signedWav) < 0.01 || std::abs(engine.transportSnapshot().positionSeconds - 1.25) > 0.01)
        return juce::Result::fail("stem was silent or transport position changed");
    tracks = engine.arrangementSnapshot();
    if (! tracks[0].soloed || ! tracks[4].muted)
        return juce::Result::fail("temporary render audibility leaked into project state");
    auto info = engine.inspectProvenance(signedWav);
    auto actions = stemActions(info);
    const auto metadata = actions[1]["parameters"]["c2paseq:midiRender"];
    juce::MemoryBlock pluginState;
    if (! pluginState.fromBase64Encoding(savedPluginState(folder))
        || ! info.c2paPresent || ! info.assetIntact
        || actions[0]["action"].toString() != "c2pa.created"
        || actions[1]["action"].toString() != "c2pa.rendered"
        || metadata["trackName"].toString() != "Actual Instrument Track"
        || static_cast<int>(metadata["clipCount"]) != 1
        || static_cast<int>(metadata["noteCount"]) != 1
        || static_cast<double>(metadata["bpm"]) != 120.0
        || metadata["instrument"]["name"].toString() != synth.name
        || metadata["instrument"]["vendor"].toString() != synth.vendor
        || metadata["instrument"]["version"].toString() != synth.version
        || metadata["instrument"]["format"].toString() != "VST3"
        || metadata["instrument"]["identifier"].toString() != synth.identifier
        || metadata["instrument"]["stateSha256"].toString()
            != juce::SHA256(pluginState).toHexString()
        || info.rawManifestJson.contains("stateBase64"))
        return juce::Result::fail("stem actions or real MIDI/instrument metadata did not round-trip");
    const auto expectedMidi = juce::JSON::parse(
        R"([{"startBeats":4.0,"lengthBeats":4.0,"notes":[{"pitch":60,"startBeats":0.0,"durationBeats":1.0,"velocity":100}]}])");
    const auto expectedMidiJson = juce::JSON::toString(expectedMidi, true);
    if (metadata["midiContentSha256"].toString() != juce::SHA256(
            expectedMidiJson.toRawUTF8(), expectedMidiJson.getNumBytesAsUTF8()).toHexString())
        return juce::Result::fail("MIDI content hash was not deterministic content-only JSON");
    juce::String newClip;
    if (engine.importMidiStem(plan, unsignedWav, newClip).wasOk())
        return juce::Result::fail("unsigned stem was imported as credentialed");
    if (engine.setTrackSolo(0, false).failed() || engine.setTrackMute(0, true).failed()
        || engine.setTrackMute(4, false).failed() || engine.setTrackSolo(4, true).failed())
        return juce::Result::fail("could not prepare mixer equivalence control");
    const auto beforeBounce = engine.exportMix(root.getChildFile("before-bounce.wav"));
    if (beforeBounce.result.failed() || ! beforeBounce.ingredients.empty())
        return juce::Result::fail("direct MIDI export failed or fabricated an ingredient");
    if (auto result = engine.importMidiStem(plan, signedWav, newClip); result.failed()) return result;
    tracks = engine.arrangementSnapshot();
    if (tracks.size() != 6 || tracks.back().name != "Actual Instrument Track Rendered Stem"
        || tracks.back().type != c2paseq::TrackType::audio || tracks.back().clips.size() != 1
        || ! tracks[4].muted || tracks[4].soloed || ! tracks.back().soloed
        || tracks.back().gainDb != -6.0 || tracks.back().pan != -0.25
        || std::abs(tracks.back().clips.front().startSeconds - 2.0) > 0.001
        || ! tracks.back().clips.front().provenance.assetIntact || newClip.isEmpty())
        return juce::Result::fail("bounce did not import a correctly aligned credentialed stem");
    const auto stemFile = tracks.back().clips.front().mediaFile;
    if (! engine.undo() || engine.arrangementSnapshot().size() != 5
        || engine.arrangementSnapshot()[4].muted || ! engine.arrangementSnapshot()[4].soloed
        || ! engine.redo()
        || engine.arrangementSnapshot().size() != 6
        || engine.saveProject().failed() || engine.openProject(folder).failed())
        return juce::Result::fail("bounce was not a single persistent undoable mutation");
    tracks = engine.arrangementSnapshot();
    if (! tracks[4].muted || ! tracks.back().clips.front().provenance.c2paPresent
        || tracks.back().clips.front().mediaFile != stemFile)
        return juce::Result::fail("stem provenance or source mute did not survive reopen");
    const auto afterBounce = engine.exportMix(root.getChildFile("after-bounce.wav"));
    if (afterBounce.result.failed() || afterBounce.ingredients.size() != 1
        || std::abs(readRms(beforeBounce.outputFile) - readRms(afterBounce.outputFile)) > 0.0001)
        return juce::Result::fail("bounced playback changed level or printed the mixer twice");
    if (engine.setTrackSolo(5, false).failed() || engine.setTrackMute(0, false).failed())
        return juce::Result::fail("could not prepare final mix acceptance");
    const auto exported = engine.exportMix(root.getChildFile("stem-final-mix.wav"));
    if (exported.result.failed()) return exported.result;
    const auto ingredient = std::find_if(exported.ingredients.begin(), exported.ingredients.end(),
        [&](const auto& item) { return item.file == stemFile; });
    const auto finalDocument = juce::JSON::parse(exported.outputProvenance.rawManifestJson);
    if (ingredient == exported.ingredients.end() || exported.ingredients.size() != 2
        || ! ingredient->provenance.c2paPresent
        || finalDocument["manifests"][juce::Identifier(info.activeManifest)].getDynamicObject() == nullptr)
        return juce::Result::fail("final export did not retain the real signed stem ingredient");
    // Stale source and bypassed instrument must fail without another imported track.
    if (engine.addMidiNote(clipId, 64, 1.0, 1.0, 100).failed()
        || engine.importMidiStem(originalPlan, signedWav, newClip).wasOk()
        || engine.setTrackPluginBypassed(4, true).failed()
        || engine.prepareMidiStem(4, plan).wasOk()
        || engine.arrangementSnapshot().size() != 6)
        return juce::Result::fail("stale/bypassed bounce did not fail cleanly");
    if (engine.setTrackPluginBypassed(4, false).failed()
        || engine.prepareMidiStem(4, plan).failed()
        || engine.renderAndSignMidiStem(plan, root, root.getChildFile("failed-stem.wav")).wasOk()
        || engine.arrangementSnapshot().size() != 6)
        return juce::Result::fail("render failure mutated the project");
    if (engine.removeSigningCredential().failed()
        || engine.renderAndSignMidiStem(plan, root.getChildFile("unsigned-sign-failure.wav"),
                                         root.getChildFile("failed-sign.wav")).wasOk()
        || root.getChildFile("failed-sign.wav").exists()
        || engine.arrangementSnapshot().size() != 6)
        return juce::Result::fail("signing failure left a success artifact or imported track");
    if (engine.configureSigningCredential(bundle).failed()
        || ! pointSavedPluginAtMissingBundle(folder) || engine.openProject(folder).failed()
        || engine.prepareMidiStem(4, plan).wasOk()
        || engine.arrangementSnapshot().size() != 6)
        return juce::Result::fail("missing instrument was accepted for bounce");
    std::cout << "MIDI stem: actual VST3 render, deterministic metadata, signing, aligned import, "
                 "undo/redo, reopen, real final-export ingredient, and failure controls passed\n";
    return juce::Result::ok();
}

juce::Result exerciseExternalVst3(const juce::File& bundle,
                                  const juce::File& source,
                                  const juce::File& root)
{
    if (! bundle.isDirectory())
        return juce::Result::fail("external VST3 bundle does not exist");

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, bundle.getFullPathName());
    auto* description = [&]() -> juce::PluginDescription*
    {
        for (auto* candidate : descriptions)
            if (! candidate->isInstrument)
                return candidate;
        return nullptr;
    }();
    if (description == nullptr)
        return juce::Result::fail("external bundle exposes no VST3 audio effect");

    juce::String error;
    auto instance = format.createInstanceFromDescription(*description, 48000.0, 512, error);
    if (instance == nullptr)
        return juce::Result::fail("external VST3 did not instantiate: " + error);
    instance->prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditorIfNeeded());

    juce::MemoryBlock state;
    instance->getStateInformation(state);
    if (state.isEmpty())
        return juce::Result::fail("external VST3 returned no restorable state");

    c2paseq::TracktionAdapter adapter;
    const auto output = root.getChildFile("external-vst3-render.wav");
    if (! adapter.createProjectEdit(root.getChildFile("external.tracktionedit"))
        || adapter.setTrackProperties(0, "External VST3", 0.0, 0.0, false, false).failed()
        || adapter.insertAudioClip(source, "source.wav", 0, 0.0, 0.0, 1.0).failed()
        || adapter.setTrackPlugin(0, *description, {}, false).failed()
        || adapter.renderWav(output, 1.0).failed()
        || readRms(output) <= 0.0)
        return juce::Result::fail("external VST3 failed in the Tracktion offline graph");

    std::cout << "External VST3 acceptance: " << description->name << " | "
              << description->manufacturerName << " | " << description->version
              << " | editor=" << (editor != nullptr ? "yes" : "generic-fallback")
              << " | state-bytes=" << state.getSize() << '\n';
    return juce::Result::ok();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;
    const auto root = juce::File::getCurrentWorkingDirectory()
        .getNonexistentChildFile("c2paseq-vst3-tests", {}, false);
    if (! root.createDirectory().wasOk())
        return fail(1, "could not create VST3 test directory");
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteRecursively(); } } cleanup { root };

    const juce::File fixture(C2PASEQ_TEST_VST3_BUNDLE);
    const juce::File synthFixture(C2PASEQ_TEST_SYNTH_VST3_BUNDLE);
    if (! fixture.isDirectory() || ! synthFixture.isDirectory())
        return fail(2, "deterministic VST3 fixtures were not built");

    const auto cache = root.getChildFile("vst3-cache.xml");
    c2paseq::PluginScanner scanner(cache);
    juce::FileSearchPath paths;
    paths.add(fixture.getParentDirectory());
    paths.add(synthFixture.getParentDirectory());
    if (auto result = scanner.scanVst3(paths); result.failed())
        return fail(3, result.getErrorMessage());
    const auto& scanned = scanner.cachedPlugins();
    const auto found = std::find_if(scanned.begin(), scanned.end(), [](const auto& plugin)
    {
        return plugin.name == "C2PA Test Gain" && plugin.format == "VST3"
            && ! plugin.isInstrument;
    });
    if (found == scanned.end() || found->vendor != "C2PA Test")
        return fail(4, "VST3 discovery did not preserve fixture metadata");
    const auto synth = std::find_if(scanned.begin(), scanned.end(), [](const auto& plugin)
    {
        return plugin.name == "C2PA Test Synth" && plugin.format == "VST3"
            && plugin.isInstrument;
    });
    if (synth == scanned.end() || synth->vendor != "C2PA Test")
        return fail(4, "VST3 instrument discovery did not preserve fixture metadata");

    const auto locatedCache = root.getChildFile("located-vst3-cache.xml");
    c2paseq::PluginScanner locatedScanner(locatedCache);
    if (auto result = locatedScanner.scanVst3Bundle(fixture); result.failed())
        return fail(4, "targeted VST3 effect discovery failed: " + result.getErrorMessage());
    if (auto result = locatedScanner.scanVst3Bundle(synthFixture); result.failed())
        return fail(4, "targeted VST3 instrument discovery failed: " + result.getErrorMessage());
    const auto& located = locatedScanner.cachedPlugins();
    const auto locatedEffect = std::find_if(located.begin(), located.end(), [](const auto& plugin)
    {
        return plugin.name == "C2PA Test Gain" && ! plugin.isInstrument;
    });
    const auto locatedSynth = std::find_if(located.begin(), located.end(), [](const auto& plugin)
    {
        return plugin.name == "C2PA Test Synth" && plugin.isInstrument;
    });
    if (locatedEffect == located.end() || locatedSynth == located.end())
        return fail(4, "targeted VST3 discovery did not preserve the existing cache");
    if (locatedScanner.scanVst3Bundle(root.getChildFile("not-a-plugin.txt")).wasOk())
        return fail(4, "targeted VST3 discovery accepted an invalid bundle");

    juce::VST3PluginFormat format;
    juce::String error;
    auto instance = format.createInstanceFromDescription(found->toJuce(), 48000.0, 512, error);
    if (instance == nullptr)
        return fail(5, "VST3 fixture did not instantiate: " + error);
    instance->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> probe(2, 512);
    probe.clear();
    for (int channel = 0; channel < probe.getNumChannels(); ++channel)
        probe.addFrom(channel, 0, std::vector<float>(512, 1.0f).data(), 512);
    juce::MidiBuffer midi;
    instance->processBlock(probe, midi);
    if (std::abs(probe.getSample(0, 0) - 0.25f) > 0.001f)
        return fail(6, "realtime VST3 processing did not change the signal");

    instance->getParameters().getUnchecked(0)->setValueNotifyingHost(0.6f);
    juce::MemoryBlock savedState;
    instance->getStateInformation(savedState);
    auto restored = format.createInstanceFromDescription(found->toJuce(), 48000.0, 512, error);
    if (restored == nullptr || savedState.isEmpty())
        return fail(6, "VST3 state could not be serialized");
    restored->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    restored->prepareToPlay(48000.0, 512);
    probe.clear();
    for (int channel = 0; channel < probe.getNumChannels(); ++channel)
        probe.addFrom(channel, 0, std::vector<float>(512, 1.0f).data(), 512);
    restored->processBlock(probe, midi);
    if (std::abs(probe.getSample(0, 0) - 0.6f) > 0.001f)
        return fail(6, "serialized VST3 parameter state was not restored");

    bool editorCloseRequested = false;
    bool editorKeyReceived = false;
    {
        c2paseq::PluginWindow editorWindow(*instance,
            [&editorCloseRequested] { editorCloseRequested = true; },
            [&editorKeyReceived](const juce::KeyPress& key)
            {
                editorKeyReceived = key.getKeyCode() == 'a';
                return editorKeyReceived;
            }, false);
        if (! editorWindow.keyPressed(juce::KeyPress('a')))
            return fail(7, "VST3 editor did not forward computer-keyboard input");
        editorWindow.closeButtonPressed();
    }
    if (! editorCloseRequested || ! editorKeyReceived)
        return fail(7, "VST3 editor lifecycle or key forwarding failed");

    c2paseq::AudioEngine engine({}, cache, false);
    if (auto result = engine.scanVst3Plugins(paths); result.failed())
        return fail(7, result.getErrorMessage());
    const auto project = root.getChildFile("Hosting Test.c2paseq");
    const auto source = root.getChildFile("source.wav");
    const auto bypassed = root.getChildFile("bypassed.wav");
    const auto processed = root.getChildFile("processed.wav");
    if (! writeTone(source) || engine.createProject(project, "Hosting Test").failed()
        || engine.importAudio(source, 0, 0.0).failed())
        return fail(8, "could not create hosted processing fixture");

    engine.seek(0.5);
    if (auto result = engine.loadTrackPlugin(0, found->identifier); result.failed())
        return fail(9, result.getErrorMessage());
    const auto afterLoad = engine.transportSnapshot();
    if (std::abs(afterLoad.positionSeconds - 0.5) > 0.02)
        return fail(10, "loading a VST3 moved the playhead");
    if (engine.setTrackPluginBypassed(0, true).failed()
        || std::abs(engine.transportSnapshot().positionSeconds - 0.5) > 0.02
        || engine.setTrackPluginBypassed(0, false).failed())
        return fail(11, "VST3 bypass changed transport state");

    const auto editorMarker = root.getChildFile("editor-lifecycle.txt");
    const auto editorMarkerPath = editorMarker.getFullPathName().toStdString();
    ::setenv("C2PASEQ_TEST_EDITOR_MARKER", editorMarkerPath.c_str(), 1);
    if (engine.openTrackPluginEditor(0).failed()
        || editorMarker.loadFileAsString() != "opened")
        return fail(12, "hosted VST3 editor did not open");
    if (engine.importAudio(source, 1, 0.0).failed()
        || editorMarker.loadFileAsString() != "closed")
        return fail(12, "arrangement rebuild did not close the VST3 editor first");
    ::unsetenv("C2PASEQ_TEST_EDITOR_MARKER");

    if (engine.saveProject().failed() || savedPluginState(project).isEmpty()
        || engine.openProject(project).failed())
        return fail(13, "project save/reopen failed with a hosted VST3");
    const auto tracks = engine.arrangementSnapshot();
    if (tracks.empty() || ! tracks[0].plugin.has_value()
        || tracks[0].plugin->identifier != found->identifier
        || tracks[0].plugin->missing || tracks[0].plugin->bypassed)
        return fail(14, "VST3 identity/state was not restored after reopen");

    c2paseq::TracktionAdapter stateAdapter;
    c2paseq::ProvenanceService stateProvenance;
    c2paseq::ProjectEngine stateProject(stateAdapter, stateProvenance);
    c2paseq::PluginHost stateHost(stateAdapter, stateProject,
                                  root.getChildFile("state-cache.xml"), false);
    const auto stateProjectFolder = root.getChildFile("State Persistence.c2paseq");
    if (stateHost.scanVst3(paths).failed()
        || stateProject.createProject(stateProjectFolder, "State Persistence").failed()
        || stateProject.importAudio(source, 0, 0.0).failed()
        || stateHost.loadTrackPlugin(0, found->identifier).failed())
        return fail(14, "could not create VST3 state persistence fixture");
    const auto initialHostedState = savedPluginState(stateProjectFolder);
    auto* stateInstance = stateAdapter.trackPluginInstance(0);
    if (stateInstance == nullptr || stateInstance->getParameters().isEmpty())
        return fail(14, "hosted VST3 parameter was unavailable");
    stateInstance->getParameters().getUnchecked(0)->setValueNotifyingHost(0.8f);
    if (std::abs(stateInstance->getParameters().getUnchecked(0)->getValue() - 0.8f) > 0.001f)
        return fail(14, "hosted VST3 parameter did not accept a live edit");
    if (stateProject.importAudio(source, 1, 0.0).failed())
        return fail(14, "arrangement rebuild failed after VST3 parameter change");
    const auto changedHostedState = savedPluginState(stateProjectFolder);
    stateInstance = stateAdapter.trackPluginInstance(0);
    const auto rebuiltValue = stateInstance != nullptr && ! stateInstance->getParameters().isEmpty()
        ? stateInstance->getParameters().getUnchecked(0)->getValue() : -1.0f;
    if (changedHostedState.isEmpty() || changedHostedState == initialHostedState)
        return fail(14, "live VST3 parameter state was not captured before rebuild; value="
            + juce::String(rebuiltValue, 3) + ", before="
            + juce::String(initialHostedState.length()) + ", after="
            + juce::String(changedHostedState.length()));
    if (stateProject.openProject(stateProjectFolder).failed())
        return fail(14, "saved VST3 parameter fixture could not reopen");
    stateInstance = stateAdapter.trackPluginInstance(0);
    if (stateInstance == nullptr || stateInstance->getParameters().isEmpty()
        || std::abs(stateInstance->getParameters().getUnchecked(0)->getValue() - 0.8f) > 0.001f)
        return fail(14, "saved VST3 parameter state was not restored after reopen");

    const auto missingProject = root.getChildFile("Missing Plugin.c2paseq");
    if (! project.copyDirectoryTo(missingProject)
        || ! pointSavedPluginAtMissingBundle(missingProject))
        return fail(15, "could not create missing-plugin project fixture");
    c2paseq::AudioEngine missingEngine({}, root.getChildFile("missing-cache.xml"));
    if (auto result = missingEngine.openProject(missingProject); result.failed())
        return fail(15, "project with missing VST3 did not open: " + result.getErrorMessage());
    const auto missingTracks = missingEngine.arrangementSnapshot();
    if (missingTracks.empty() || missingTracks[0].clips.empty()
        || ! missingTracks[0].plugin.has_value() || ! missingTracks[0].plugin->missing
        || ! missingTracks[0].plugin->bypassed
        || missingEngine.removeTrackPlugin(0).failed())
        return fail(15, "missing VST3 was not preserved, bypassed, and removable");

    c2paseq::TracktionAdapter direct;
    if (! direct.createProjectEdit(root.getChildFile("render.tracktionedit"))
        || direct.setTrackProperties(0, "Audio 1", 0.0, 0.0, false, false).failed()
        || direct.insertAudioClip(source, "source.wav", 0, 0.0, 0.0, 1.0).failed()
        || direct.setTrackPlugin(0, found->toJuce(), {}, false).failed()
        || direct.setTrackPluginBypassed(0, true).failed()
        || direct.renderWav(bypassed, 1.0).failed()
        || direct.setTrackPluginBypassed(0, false).failed()
        || direct.renderWav(processed, 1.0).failed())
        return fail(16, "Tracktion hosted offline render failed");
    const auto inputRms = readRms(bypassed);
    const auto outputRms = readRms(processed);
    if (inputRms <= 0.0 || std::abs(outputRms / inputRms - 0.25) > 0.04)
        return fail(17, "offline render did not contain the hosted VST3 result; ratio="
            + juce::String(outputRms / inputRms, 4));

    engine.seek(0.5);
    if (engine.removeTrackPlugin(0).failed()
        || std::abs(engine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(18, "removing a VST3 moved the playhead");

    const auto originalTrackCount = engine.arrangementSnapshot().size();
    if (engine.addAudioTrack().failed())
        return fail(19, "could not add a track for hosted VST3 validation");
    const auto addedTrackIndex = static_cast<int>(originalTrackCount);
    if (engine.loadTrackPlugin(addedTrackIndex, found->identifier).failed())
        return fail(19, "new audio track did not accept a VST3 effect");
    const auto withAddedPluginTrack = engine.arrangementSnapshot();
    if (withAddedPluginTrack.size() != originalTrackCount + 1
        || ! withAddedPluginTrack.back().plugin.has_value()
        || engine.deleteAudioTrack(addedTrackIndex).failed()
        || engine.arrangementSnapshot().size() != originalTrackCount
        || engine.saveProject().failed() || engine.openProject(project).failed()
        || engine.arrangementSnapshot().size() != originalTrackCount)
        return fail(19, "VST3-bearing track add/delete did not persist cleanly");

    c2paseq::AudioEngine midiEngine({}, cache, false);
    if (auto result = midiEngine.scanVst3Plugins(paths); result.failed())
        return fail(20, result.getErrorMessage());
    const auto midiProject = root.getChildFile("MIDI Instrument Test.c2paseq");
    if (midiEngine.createProject(midiProject, "MIDI Instrument Test").failed()
        || midiEngine.addMidiTrack().failed())
        return fail(21, "could not create MIDI instrument fixture");
    const auto midiTrackIndex = static_cast<int>(midiEngine.arrangementSnapshot().size()) - 1;
    if (midiEngine.loadTrackPlugin(midiTrackIndex, found->identifier).wasOk()
        || midiEngine.loadTrackPlugin(0, synth->identifier).wasOk())
        return fail(22, "track type did not reject the wrong VST3 role");
    if (midiEngine.createMidiClip(midiTrackIndex, 0.0, 4.0).failed())
        return fail(23, "could not create MIDI playback clip");
    auto midiTracks = midiEngine.arrangementSnapshot();
    const auto midiClipId = midiTracks[static_cast<std::size_t>(midiTrackIndex)]
                                .midiClips.front().id;
    if (midiEngine.addMidiNote(midiClipId, 60, 0.0, 1.0, 100).failed()
        || midiEngine.addMidiNote(midiClipId, 64, 1.0, 1.0, 100).failed()
        || midiEngine.addMidiNote(midiClipId, 67, 2.0, 1.0, 100).failed())
        return fail(23, "could not add MIDI playback notes");

    midiEngine.seek(0.5);
    if (midiEngine.loadTrackPlugin(midiTrackIndex, synth->identifier).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02
        || midiEngine.loadTrackPlugin(midiTrackIndex, synth->identifier).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(24, "loading or replacing an instrument changed transport state");
    midiTracks = midiEngine.arrangementSnapshot();
    const auto& midiTrack = midiTracks[static_cast<std::size_t>(midiTrackIndex)];
    if (! midiTrack.plugin.has_value() || midiTrack.plugin->identifier != synth->identifier
        || midiTrack.plugin->missing || midiTrack.plugin->bypassed)
        return fail(24, "MIDI track did not expose exactly one loaded instrument");
    if (midiEngine.sendLiveMidiMessage(0,
            juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100))).wasOk()
        || midiEngine.sendLiveMidiMessage(midiTrackIndex,
            juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100))).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(24, "live MIDI monitoring routing failed or changed transport state");
    if (midiEngine.sendLiveMidiMessage(midiTrackIndex,
            juce::MidiMessage::noteOff(1, 60)).failed())
        return fail(24, "live MIDI note-off routing failed");
    midiEngine.allNotesOff(midiTrackIndex);
    if (midiEngine.setTrackGain(midiTrackIndex, -6.0).failed()
        || midiEngine.setTrackPan(midiTrackIndex, -0.25).failed()
        || midiEngine.setTrackMute(midiTrackIndex, true).failed()
        || midiEngine.setTrackMute(midiTrackIndex, false).failed()
        || midiEngine.setTrackSolo(midiTrackIndex, true).failed()
        || midiEngine.setTrackSolo(midiTrackIndex, false).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(24, "MIDI instrument mixer controls failed or changed transport state");
    midiTracks = midiEngine.arrangementSnapshot();
    if (std::abs(midiTracks[static_cast<std::size_t>(midiTrackIndex)].gainDb + 6.0) > 0.001
        || std::abs(midiTracks[static_cast<std::size_t>(midiTrackIndex)].pan + 0.25) > 0.001)
        return fail(24, "MIDI instrument mixer values were not retained");
    if (midiEngine.setTrackPluginBypassed(midiTrackIndex, true).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02
        || midiEngine.setTrackPluginBypassed(midiTrackIndex, false).failed()
        || std::abs(midiEngine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(24, "bypassing a MIDI instrument changed transport state");
    if (midiEngine.saveProject().failed() || savedPluginState(midiProject).isEmpty()
        || midiEngine.openProject(midiProject).failed())
        return fail(25, "MIDI instrument state did not save and reopen");
    midiTracks = midiEngine.arrangementSnapshot();
    if (! midiTracks[static_cast<std::size_t>(midiTrackIndex)].plugin.has_value()
        || midiTracks[static_cast<std::size_t>(midiTrackIndex)].plugin->missing)
        return fail(25, "MIDI instrument identity was not restored after reopen");

    c2paseq::TracktionAdapter midiRenderAdapter;
    const auto midiRender = root.getChildFile("midi-instrument-render.wav");
    const std::vector<c2paseq::MidiPlaybackNote> midiNotes {
        { 60, 0.0, 1.0, 100 }, { 64, 1.0, 1.0, 100 }, { 67, 2.0, 1.0, 100 }
    };
    if (! midiRenderAdapter.createProjectEdit(root.getChildFile("midi-render.tracktionedit"))
        || midiRenderAdapter.setTrackProperties(0, "MIDI 1", 0.0, 0.0,
                                                false, false).failed()
        || midiRenderAdapter.insertMidiClip("MIDI Clip", 0, 0.0, 4.0,
                                            midiNotes).failed()
        || midiRenderAdapter.setTrackPlugin(0, synth->toJuce(), {}, false).failed())
        return fail(26, "could not build the arranged MIDI instrument graph");

    c2paseq::Project renderProject = c2paseq::Project::create("MIDI Render");
    renderProject.tracks.clear();
    c2paseq::TrackModel renderTrack;
    renderTrack.id = juce::Uuid().toString();
    renderTrack.name = "MIDI 1";
    renderTrack.type = c2paseq::TrackType::midi;
    c2paseq::MidiClipModel renderClip;
    renderClip.id = juce::Uuid().toString();
    renderClip.start.beats = 0.0;
    renderClip.length.beats = 4.0;
    for (const auto& note : midiNotes)
        renderClip.notes.push_back({ juce::Uuid().toString(), note.noteNumber,
            { note.startBeats }, { note.durationBeats }, note.velocity });
    renderTrack.midiClips.push_back(std::move(renderClip));
    renderProject.tracks.push_back(std::move(renderTrack));
    c2paseq::PluginState renderPlugin;
    renderPlugin.ownerId = renderProject.tracks.front().id;
    renderPlugin.pluginIdentifier = synth->identifier;
    renderPlugin.name = synth->name;
    renderPlugin.format = "VST3";
    renderPlugin.isInstrument = true;
    renderProject.plugins.push_back(std::move(renderPlugin));
    c2paseq::RenderPlan renderPlan;
    const c2paseq::ProjectPaths renderPaths(root.getChildFile("render-plan.c2paseq"));
    if (c2paseq::RenderService::createPlan(renderProject, renderPaths, renderPlan).failed()
        || ! renderPlan.ingredients.empty() || std::abs(renderPlan.endSeconds - 2.0) > 0.01
        || c2paseq::RenderService::render(midiRenderAdapter, renderPlan, midiRender).failed()
        || readRms(midiRender) <= 0.01)
        return fail(27, "offline export did not contain instrument-rendered MIDI audio");
    const auto isolatedRender = root.getChildFile("selected-midi-only.wav");
    const auto selectedBaseline = root.getChildFile("selected-midi-baseline.wav");
    if (midiRenderAdapter.renderTrackWav(selectedBaseline, 0, 2.0).failed())
        return fail(27, "could not render the selected-track baseline without master processing");
    if (midiRenderAdapter.setTrackProperties(1, "Unrelated solo", 0.0, 0.0,
                                            false, true).failed()
        || midiRenderAdapter.insertAudioClip(source, "unrelated.wav", 1, 0.0, 0.0, 1.0).failed()
        || midiRenderAdapter.setTrackMute(0, true).failed())
        return fail(27, "could not prepare isolated-track acceptance controls");
    if (const auto result = midiRenderAdapter.renderTrackWav(isolatedRender, 0, 2.0);
        result.failed()) return fail(27, result.getErrorMessage());
    if (std::abs(readRms(isolatedRender) - readRms(selectedBaseline)) > 0.0001)
        return fail(27, "isolated RMS=" + juce::String(readRms(isolatedRender), 8)
            + " baseline RMS=" + juce::String(readRms(selectedBaseline), 8));
    if (midiRenderAdapter.renderTrackWav(root, 0, 2.0).wasOk()
        || midiRenderAdapter.renderTrackWav(isolatedRender, -1, 2.0).wasOk()
        || midiRenderAdapter.renderTrackWav(isolatedRender, 0, 1.0, 2.0).wasOk())
        return fail(27, "selected-track render accepted an invalid destination, index, or range");

    const auto externalPath = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_EXTERNAL_VST3", {});
    if (externalPath.isNotEmpty())
        if (auto result = exerciseExternalVst3(juce::File(externalPath), source, root);
            result.failed())
            return fail(28, result.getErrorMessage());

    if (auto result = exerciseMidiStemBounce(*synth, source, root, cache); result.failed())
        return fail(29, result.getErrorMessage());

    std::cout << "VST3 scan, instantiate, realtime DSP, attach, bypass, persistence, "
                 "editor lifecycle, offline DSP, dynamic track hosting, MIDI instrument routing, "
                 "replacement, rendered audio, remove, and transport invariants passed\n";
    return 0;
}

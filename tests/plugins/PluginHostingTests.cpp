#include "engine/AudioEngine.h"
#include "plugins/PluginScanner.h"

#include <cmath>
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
    if (! fixture.isDirectory())
        return fail(2, "deterministic VST3 fixture was not built");

    const auto cache = root.getChildFile("vst3-cache.xml");
    c2paseq::PluginScanner scanner(cache);
    juce::FileSearchPath paths;
    paths.add(fixture.getParentDirectory());
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

    c2paseq::AudioEngine engine({}, cache);
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

    if (engine.saveProject().failed() || savedPluginState(project).isEmpty()
        || engine.openProject(project).failed())
        return fail(12, "project save/reopen failed with a hosted VST3");
    const auto tracks = engine.arrangementSnapshot();
    if (tracks.empty() || ! tracks[0].plugin.has_value()
        || tracks[0].plugin->identifier != found->identifier
        || tracks[0].plugin->missing || tracks[0].plugin->bypassed)
        return fail(13, "VST3 identity/state was not restored after reopen");

    const auto missingProject = root.getChildFile("Missing Plugin.c2paseq");
    if (! project.copyDirectoryTo(missingProject)
        || ! pointSavedPluginAtMissingBundle(missingProject))
        return fail(13, "could not create missing-plugin project fixture");
    c2paseq::AudioEngine missingEngine({}, root.getChildFile("missing-cache.xml"));
    if (auto result = missingEngine.openProject(missingProject); result.failed())
        return fail(13, "project with missing VST3 did not open: " + result.getErrorMessage());
    const auto missingTracks = missingEngine.arrangementSnapshot();
    if (missingTracks.empty() || missingTracks[0].clips.empty()
        || ! missingTracks[0].plugin.has_value() || ! missingTracks[0].plugin->missing
        || ! missingTracks[0].plugin->bypassed
        || missingEngine.removeTrackPlugin(0).failed())
        return fail(13, "missing VST3 was not preserved, bypassed, and removable");

    c2paseq::TracktionAdapter direct;
    if (! direct.createProjectEdit(root.getChildFile("render.tracktionedit"))
        || direct.setTrackProperties(0, "Audio 1", 0.0, 0.0, false, false).failed()
        || direct.insertAudioClip(source, "source.wav", 0, 0.0, 0.0, 1.0).failed()
        || direct.setTrackPlugin(0, found->toJuce(), {}, false).failed()
        || direct.setTrackPluginBypassed(0, true).failed()
        || direct.renderWav(bypassed, 1.0).failed()
        || direct.setTrackPluginBypassed(0, false).failed()
        || direct.renderWav(processed, 1.0).failed())
        return fail(14, "Tracktion hosted offline render failed");
    const auto inputRms = readRms(bypassed);
    const auto outputRms = readRms(processed);
    if (inputRms <= 0.0 || std::abs(outputRms / inputRms - 0.25) > 0.04)
        return fail(15, "offline render did not contain the hosted VST3 result; ratio="
            + juce::String(outputRms / inputRms, 4));

    engine.seek(0.5);
    if (engine.removeTrackPlugin(0).failed()
        || std::abs(engine.transportSnapshot().positionSeconds - 0.5) > 0.02)
        return fail(16, "removing a VST3 moved the playhead");

    const auto externalPath = juce::SystemStats::getEnvironmentVariable(
        "C2PASEQ_EXTERNAL_VST3", {});
    if (externalPath.isNotEmpty())
        if (auto result = exerciseExternalVst3(juce::File(externalPath), source, root);
            result.failed())
            return fail(17, result.getErrorMessage());

    std::cout << "VST3 scan, instantiate, realtime DSP, attach, bypass, persistence, "
                 "offline DSP, remove, and transport invariants passed\n";
    return 0;
}

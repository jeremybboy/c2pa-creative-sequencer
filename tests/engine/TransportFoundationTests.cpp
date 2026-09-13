#include "engine/NativeAudioClipPolicy.h"
#include "transport/TransportFormatting.h"

#include <tracktion_engine/tracktion_engine.h>

#include <cmath>
#include <iostream>
#include <memory>

namespace te = tracktion::engine;

namespace
{
class HostedOnlyEngineBehaviour final : public te::EngineBehaviour
{
public:
    bool autoInitialiseDeviceManager() override
    {
        return false;
    }

    bool addSystemAudioIODeviceTypes() override
    {
        return false;
    }

    bool shouldOpenAudioInputByDefault() override
    {
        return false;
    }
};

bool isSilent(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        if (buffer.getMagnitude(channel, 0, buffer.getNumSamples()) != 0.0f)
            return false;

    return true;
}

bool writeTestAudio(const juce::File& file,
                    juce::AudioFormat& format,
                    double sampleRate = 44100.0,
                    int numSamples = 11025,
                    const juce::StringPairArray& metadata = {})
{
    juce::AudioBuffer<float> source(1, numSamples);
    for (int sample = 0; sample < numSamples; ++sample)
        source.setSample(0, sample, 0.35f * std::sin(
            juce::MathConstants<double>::twoPi * 440.0 * sample / sampleRate));

    if (! file.getParentDirectory().createDirectory())
    {
        std::cerr << "could not create fixture directory "
                  << file.getParentDirectory().getFullPathName() << '\n';
        return false;
    }
    auto stream = file.createOutputStream();
    if (stream == nullptr)
    {
        std::cerr << "could not open fixture stream " << file.getFullPathName() << '\n';
        return false;
    }
    auto writer = std::unique_ptr<juce::AudioFormatWriter>(
        format.createWriterFor(stream.release(), sampleRate, 1, 16, metadata, 0));
    if (writer == nullptr)
    {
        std::cerr << "could not create " << format.getFormatName() << " writer\n";
        return false;
    }
    if (! writer->writeFromAudioSampleBuffer(source, 0, numSamples))
    {
        std::cerr << "could not write " << format.getFormatName() << " samples\n";
        return false;
    }
    return true;
}

bool writeEmbeddedMp3(const juce::File& file)
{
    constexpr auto encoded =
        "SUQzBAAAAAAAIlRTU0UAAAAOAAADTGF2ZjYxLjcuMTAwAAAAAAAAAAAAAAD/4zjAAAAAAAAAAAAASW5mbwAAAA8AAAAEAAADGAB0dHR0dHR0dHR0dHR0dHR0dHR0dHR0dHSioqKioqKioqKioqKioqKioqKioqKioqKi0dHR0dHR0dHR0dHR0dHR0dHR0dHR0dHR0f////////////////////////////////8AAAAATGF2YzYxLjE5AAAAAAAAAAAAAAAAJAOgAAAAAAAAAxhYaZKRAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/4yjEAB0oqohfTwAAC5tyUAB+/fv379/e9IDx48ePKUu/fv37/fhskEv4X4DuAtgQwwzjrejx48BCsHz+UOfKB/o9+sHDmIAffWDgYyAPvrAhzQD/KO78HwcBAEAQBAHwfB8HwICAIAgGAfB8PygIBjf/4Pg+BAQBAEHAcHwffUCCTSLvAEDITf////8AAEz/4yjEDh2p2mwBnJAADBEKgQwWEv3+P//mIx0YEDZ6hVGhy9pOg9WkggoeIQGd0pYGIYBGgeSBtIbMFkQWFf/ikQ+UNWjlCghQRDRcv//i5SaHOHOKJFSKmRFiLf//5iXS6ZF4vIl0upA1/+JQkDQlOnv//+xdcDohdotFotFotHK9vjgvy8TSf//ldvcNqWr/4yjEGiPZ8s5Zm4ACDQWAUKf//ddY5rMYEAE06KpXF1/+O4PgFgJgZQiABsIGC+IjoexHP5cIoVC4XDQuFwcorjKk0bkBLyf+gXC4yCDIIMRUyWTKSy6ksu/+ghQQoIUGqWXYKlToK/hgo44UnCp0FSp0NCX+UuRcjGhoSjTolWdEqkxBTUUzLjEwMKqqqqr/4yjEDQAAA0gBwAAAqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=";
    juce::MemoryOutputStream bytes;
    if (! juce::Base64::convertFromBase64(bytes, encoded))
        return false;
    return file.replaceWithData(bytes.getData(), bytes.getDataSize());
}

struct ScopedTestDirectory
{
    ScopedTestDirectory()
        : file(juce::File::getCurrentWorkingDirectory()
            .getNonexistentChildFile("c2paseq-audio-import", {}, false))
    {
        file.createDirectory();
    }

    ~ScopedTestDirectory()
    {
        file.deleteRecursively();
    }

    juce::File file;
};
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (c2paseq::transport::formatPosition(65.432) != "01:05.432")
        return 1;

    if (c2paseq::transport::sanitisePosition(-4.0) != 0.0)
        return 2;

    te::Engine engine("C2PATransportTests", nullptr,
                      std::make_unique<HostedOnlyEngineBehaviour>());
    auto& deviceManager = engine.getDeviceManager();
    auto& audioInterface = deviceManager.getHostedAudioDeviceInterface();
    te::HostedAudioDeviceInterface::Parameters parameters;
    parameters.sampleRate = c2paseq::transport::preferredSampleRate;
    parameters.blockSize = c2paseq::transport::preferredBlockSize;
    parameters.inputChannels = 0;
    parameters.outputChannels = 2;
    audioInterface.initialise(parameters);
    audioInterface.prepareToPlay(parameters.sampleRate, parameters.blockSize);
    deviceManager.dispatchPendingUpdates();

    auto edit = te::createEmptyEdit(engine, {});
    auto& transport = edit->getTransport();
    transport.ensureContextAllocated();

    transport.setPosition(tracktion::TimePosition::fromSeconds(3.25));
    if (std::abs(transport.getPosition().inSeconds() - 3.25) > 0.000001)
        return 3;

    transport.stop(false, false);
    transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));
    transport.play(false);

    juce::AudioBuffer<float> audio(parameters.outputChannels, parameters.blockSize);
    juce::MidiBuffer midi;
    constexpr int blocksToProcess = 64;

    for (int block = 0; block < blocksToProcess; ++block)
    {
        audio.clear();
        audioInterface.processBlock(audio, midi);

        if (! isSilent(audio))
            return 4;
    }

    const auto expectedPosition = static_cast<double>(blocksToProcess * parameters.blockSize)
        / parameters.sampleRate;
    const auto* playbackContext = transport.getCurrentPlaybackContext();
    if (playbackContext == nullptr)
        return 5;

    if (std::abs(playbackContext->getPosition().inSeconds() - expectedPosition) > 0.02)
    {
        std::cerr << "expected position " << expectedPosition
                  << ", got " << playbackContext->getPosition().inSeconds() << '\n';
        return 6;
    }

    transport.stop(false, false);
    transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));

    for (int block = 0; block < 4; ++block)
    {
        audio.clear();
        audioInterface.processBlock(audio, midi);
    }

    if (std::abs(transport.getPosition().inSeconds()) > 0.000001)
    {
        std::cerr << "stop position was " << transport.getPosition().inSeconds() << '\n';
        return 7;
    }

    ScopedTestDirectory testDirectory;
    const auto wavFile = testDirectory.file.getChildFile("tone.wav");
    const auto loopTaggedWavFile = testDirectory.file.getChildFile("loop-125-Csharp.wav");
    const auto aiffFile = testDirectory.file.getChildFile("tone.aiff");
    const auto mp3File = testDirectory.file.getChildFile("tone.mp3");
    juce::WavAudioFormat wavFormat;
    juce::AiffAudioFormat aiffFormat;
    if (! writeTestAudio(wavFile, wavFormat))
    {
        std::cerr << "failed to write WAV fixture\n";
        return 8;
    }
    juce::StringPairArray loopMetadata;
    loopMetadata.set(juce::WavAudioFormat::acidOneShot, "0");
    loopMetadata.set(juce::WavAudioFormat::acidRootSet, "1");
    loopMetadata.set(juce::WavAudioFormat::acidStretch, "1");
    loopMetadata.set(juce::WavAudioFormat::acidizerFlag, "1");
    loopMetadata.set(juce::WavAudioFormat::acidRootNote, "61");
    loopMetadata.set(juce::WavAudioFormat::acidBeats, "4");
    loopMetadata.set(juce::WavAudioFormat::acidDenominator, "4");
    loopMetadata.set(juce::WavAudioFormat::acidNumerator, "4");
    loopMetadata.set(juce::WavAudioFormat::acidTempo, "125");
    if (! writeTestAudio(loopTaggedWavFile, wavFormat, 48000.0, 92160, loopMetadata))
    {
        std::cerr << "failed to write loop-tagged WAV fixture\n";
        return 8;
    }
    if (! writeTestAudio(aiffFile, aiffFormat))
    {
        std::cerr << "failed to write AIFF fixture\n";
        return 8;
    }
    if (! writeEmbeddedMp3(mp3File))
    {
        std::cerr << "failed to write MP3 fixture\n";
        return 8;
    }

    for (const auto& file : { wavFile, aiffFile, mp3File })
    {
        te::AudioFile imported(engine, file);
        if (! imported.isValid() || imported.getLength() <= 0.0)
        {
            std::cerr << "failed to decode " << file.getFileName() << '\n';
            return 9;
        }
    }

    juce::AudioThumbnail thumbnail(
        256, engine.getAudioFileFormatManager().readFormatManager,
        engine.getAudioFileManager().getAudioThumbnailCache());
    if (! thumbnail.setSource(new juce::FileInputSource(wavFile)))
        return 10;
    const auto thumbnailDeadline = juce::Time::getMillisecondCounter() + 3000;
    while (! thumbnail.isFullyLoaded()
           && juce::Time::getMillisecondCounter() < thumbnailDeadline)
        juce::Thread::sleep(5);
    if (! thumbnail.isFullyLoaded() || thumbnail.getTotalLength() <= 0.0)
        return 11;

    const auto playbackFile = argc > 1 ? juce::File::getCurrentWorkingDirectory()
                                             .getChildFile(juce::String::fromUTF8(argv[1]))
                                       : loopTaggedWavFile;
    auto* audioTrack = te::getAudioTracks(*edit)[0];
    te::AudioFile wavAudio(engine, playbackFile);
    const te::ClipPosition clipPosition {
        { tracktion::TimePosition::fromSeconds(0.0),
          tracktion::TimePosition::fromSeconds(wavAudio.getLength()) },
        {}
    };
    const auto insertedClip = audioTrack->insertWaveClip(
        playbackFile.getFileNameWithoutExtension(), playbackFile, clipPosition, false);
    if (insertedClip == nullptr)
        return 12;

    if (! insertedClip->getAutoTempo() || ! insertedClip->getAutoPitch())
    {
        std::cerr << "loop metadata did not reproduce Tracktion auto-stretch\n";
        return 13;
    }

    std::cout << "playback fixture: " << playbackFile.getFullPathName() << '\n'
              << "source seconds: " << wavAudio.getLength() << '\n'
              << "clip seconds: " << insertedClip->getPosition().getLength().inSeconds() << '\n'
              << "auto tempo: " << insertedClip->getAutoTempo() << '\n'
              << "auto pitch: " << insertedClip->getAutoPitch() << '\n'
              << "stretch mode: " << static_cast<int>(insertedClip->getActualTimeStretchMode())
              << '\n';

    c2paseq::configureNativeAudioClip(*insertedClip, wavAudio.getLength());
    std::cout << "native clip seconds: "
              << insertedClip->getPosition().getLength().inSeconds() << '\n'
              << "native auto tempo: " << insertedClip->getAutoTempo() << '\n'
              << "native auto pitch: " << insertedClip->getAutoPitch() << '\n'
              << "native stretch mode: "
              << static_cast<int>(insertedClip->getActualTimeStretchMode()) << '\n';
    if (insertedClip->getAutoTempo()
        || insertedClip->getAutoPitch()
        || insertedClip->getActualTimeStretchMode() != te::TimeStretcher::disabled
        || std::abs(insertedClip->getPosition().getLength().inSeconds()
                    - wavAudio.getLength()) > 0.000001)
    {
        std::cerr << "native playback policy did not remove auto-stretch\n";
        return 14;
    }

    transport.stop(false, true);
    transport.ensureContextAllocated(true);
    transport.setPosition(tracktion::TimePosition::fromSeconds(0.0));
    transport.play(false);
    float peakMagnitude = 0.0f;
    for (int block = 0; block < 64; ++block)
    {
        audio.clear();
        audioInterface.processBlock(audio, midi);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            peakMagnitude = std::max(
                peakMagnitude,
                audio.getMagnitude(channel, 0, audio.getNumSamples()));
    }
    std::cout << "output peak: " << peakMagnitude << '\n';
    if (peakMagnitude == 0.0f)
        return 15;

    std::cout << "transport and audio import: WAV/AIFF/MP3 decode, waveform, and playback passed\n";
    return 0;
}

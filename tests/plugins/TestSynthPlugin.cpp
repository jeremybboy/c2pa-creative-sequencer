#include <JuceHeader.h>

#include <array>
#include <cmath>

namespace
{
class TestSynthProcessor final : public juce::AudioProcessor
{
public:
    TestSynthProcessor()
        : juce::AudioProcessor(BusesProperties()
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          level(new juce::AudioParameterFloat({ "level", 1 }, "Level",
                                               0.0f, 0.5f, 0.2f))
    {
        addParameter(level);
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double newSampleRate, int) override
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        active.fill(false);
        phases.fill(0.0);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet().isDisabled()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();
        auto iterator = midi.cbegin();
        const auto end = midi.cend();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            while (iterator != end && (*iterator).samplePosition <= sample)
            {
                const auto message = (*iterator).getMessage();
                if (message.isNoteOn())
                    active[static_cast<std::size_t>(message.getNoteNumber())] = true;
                else if (message.isNoteOff() || message.isAllNotesOff())
                {
                    if (message.isAllNotesOff())
                        active.fill(false);
                    else
                        active[static_cast<std::size_t>(message.getNoteNumber())] = false;
                }
                ++iterator;
            }

            double value = 0.0;
            auto voiceCount = 0;
            for (std::size_t note = 0; note < active.size(); ++note)
            {
                if (! active[note])
                    continue;
                const auto frequency = 440.0 * std::pow(2.0,
                    (static_cast<double>(note) - 69.0) / 12.0);
                value += std::sin(phases[note]);
                phases[note] = std::fmod(phases[note]
                    + juce::MathConstants<double>::twoPi * frequency / sampleRate,
                    juce::MathConstants<double>::twoPi);
                ++voiceCount;
            }
            if (voiceCount > 0)
                value = value * static_cast<double>(level->get()) / voiceCount;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, static_cast<float>(value));
        }
    }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor(*this);
    }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override
    {
        juce::MemoryOutputStream stream(destination, false);
        stream.writeFloat(level->get());
    }

    void setStateInformation(const void* data, int size) override
    {
        if (size < static_cast<int>(sizeof(float)))
            return;
        juce::MemoryInputStream stream(data, static_cast<std::size_t>(size), false);
        level->setValueNotifyingHost(level->convertTo0to1(stream.readFloat()));
    }

private:
    juce::AudioParameterFloat* level;
    double sampleRate = 48000.0;
    std::array<bool, 128> active {};
    std::array<double, 128> phases {};
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestSynthProcessor();
}

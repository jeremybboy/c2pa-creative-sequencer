#include <JuceHeader.h>

#include <cstdlib>

namespace
{
class TestGainEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TestGainEditor(juce::AudioProcessor& processor)
        : juce::AudioProcessorEditor(processor)
    {
        setSize(320, 180);
        writeMarker("opened");
    }

    ~TestGainEditor() override
    {
        writeMarker("closed");
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colours::darkgrey);
    }

private:
    static void writeMarker(const juce::String& state)
    {
        if (const auto* path = std::getenv("C2PASEQ_TEST_EDITOR_MARKER"); path != nullptr)
            juce::File(juce::String::fromUTF8(path)).replaceWithText(state);
    }
};

class TestGainProcessor final : public juce::AudioProcessor
{
public:
    TestGainProcessor()
        : juce::AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          gain(new juce::AudioParameterFloat({ "gain", 1 }, "Gain", 0.0f, 1.0f, 0.25f))
    {
        addParameter(gain);
    }

    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain(gain->get());
    }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new TestGainEditor(*this);
    }
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override
    {
        juce::MemoryOutputStream stream(destination, false);
        stream.writeFloat(gain->get());
    }

    void setStateInformation(const void* data, int size) override
    {
        if (size < static_cast<int>(sizeof(float)))
            return;
        juce::MemoryInputStream stream(data, static_cast<std::size_t>(size), false);
        gain->setValueNotifyingHost(gain->convertTo0to1(stream.readFloat()));
    }

private:
    juce::AudioParameterFloat* gain;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestGainProcessor();
}

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace c2paseq
{
class ArrangementView final : public juce::Component
{
public:
    explicit ArrangementView(juce::String engineStatus);

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    juce::Label title;
    juce::Label emptyState;
    juce::Label status;
};
}
